(ns editors.candy
  (:require [clojure.edn :as edn] 
            [clojure.pprint :refer [pprint]]
            [plumbing.core :refer [fnk defnk]]
            [schema.core :as s]
            [dynamo.background :as background]
            [dynamo.buffers :refer :all]
            [dynamo.camera :refer :all]
            [dynamo.editors :as ed]
            [dynamo.file :as file]
            [dynamo.file.protobuf :as protobuf :refer [pb->str]]
            [dynamo.geom :as g]
            [dynamo.gl :as gl]
            [dynamo.gl.shader :as shader]
            [dynamo.gl.texture :as texture]
            [dynamo.gl.vertex :as vtx]
            [dynamo.grid :as grid]
            [dynamo.image :refer :all]
            [dynamo.node :as n]
            [dynamo.project :as p]
            [dynamo.property :as dp]
            [dynamo.selection :as sel]
            [dynamo.system :as ds]
            [internal.ui.scene-editor :as ius]
            [dynamo.texture :as tex]
            [dynamo.types :as t :refer :all]
            [dynamo.ui :refer :all]
            [internal.render.pass :as pass])
  (:import  [com.jogamp.opengl.util.awt TextRenderer]
            [dynamo.types Animation Camera Image TextureSet Rect AABB]
            [javax.media.opengl GL GL2]
            [javax.media.opengl.glu GLU]
            [javax.vecmath Point3d]))

(set! *warn-on-reflection* true)

(defn load-level [str]
  (edn/read-string str))

(def test-level
  {:width 2
   :height 2
   :game-mode "foo"
   :blocks {[0 0] "red_candy"
            [0 1] "blue_candy"
            [1 0] "yellow_candy"
            [1 1] "red_candy"}})

(defn- get-image-color [image]
  (case image
        "red_candy" [0.8 0 0 1]
        "blue_candy" [0 0 0.8 1]
        "yellow_candy" [0.8 0.8 0 1]
        [0.8 0.8 0.8 1]))

(defn- get-selected-image-color [image]
  (case image
        "red_candy" [1.0 0 0 1]
        "blue_candy" [0 0 1.0 1]
        "yellow_candy" [1.0 1.0 0 1]
        [1.0 1.0 1.0 1]))

(vtx/defvertex texture-vtx
  (vec4 position)
  (vec2 texcoord0))

(shader/defshader vertex-shader
  (attribute vec4 position)
  (attribute vec2 texcoord0)
  (varying vec2 var_texcoord0)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
    (setq var_texcoord0 texcoord0)))

(shader/defshader fragment-shader
  (varying vec2 var_texcoord0)
  (uniform sampler2D texture)
  (uniform vec4 color)
  (defn void main []
    (setq gl_FragColor (texture2D texture var_texcoord0.xy))
    #_(setq gl_FragColor (vec4 var_texcoord0 0 1))))

(def shader (shader/make-shader vertex-shader fragment-shader))

(defn- frange [n step]
  (map #(* step %) (range n)))

(def palette-columns 2)
(def group-spacing 20)
(def cell-size 10)
(def cell-size-half (/ cell-size 2.0))
(def palette-cell-size 80)
(def palette-cell-size-half (/ palette-cell-size 2.0))

(def candy-colors ["red" "blue" "yellow" "green" "orange" "purple"])

(def palette
  [{:name "Regular"
    :images (map (fn [color] (str color "_candy")) candy-colors)}
   {:name "Striped"
    :images (map (fn [color] (str color "_polka_horisontal")) candy-colors)}
   #_DEMO
   #_{:name "Drop"
     :images ["cherry" "hazelnut"]}])

(defn- hit? [cell pos cell-size-half]
  (let [xc (:x cell)
        yc (:y cell)
        d cell-size-half]
    (when (and (<= (- xc d) (:x pos) (+ xc d))
               (<= (- yc d) (:y pos) (+ yc d)))
      cell)))

(defn- layout-row [row x y cell-size]
  (let [column-f (fn [idx image] { :x (+ x (* cell-size idx)) :y y :image image})]
    (map-indexed column-f row)))

(defn- layout-group [group x y cell-size]
  (let [row-f (fn [idx row] (layout-row row x (+ y (* idx cell-size)) cell-size))
        images (:images group)
        rows (partition-all palette-columns images)]
    {:name (:name group)
     :x x :y y
     :height (* cell-size (count rows))
     :cells (flatten (map-indexed row-f rows))}))

(defn- layout-palette
  ([groups]
    (layout-palette groups (+ (* 0.5 group-spacing) palette-cell-size-half) (+ group-spacing palette-cell-size-half)))
  ([groups x y]
    (when (seq groups)
      (let [g' (layout-group (first groups) x y palette-cell-size)]
        (conj (layout-palette (rest groups) x
                              (+ y group-spacing (:height g'))) g')))))

(defn render-text
  [ctx ^GL2 gl ^TextRenderer text-renderer ^String chars ^Float xloc ^Float yloc ^Float zloc ^Float scale]
  (gl/gl-push-matrix gl
    (.glScaled gl 1 -1 1)
    (.setColor text-renderer 1 1 1 1)
    (.begin3DRendering text-renderer)
    (.draw3D text-renderer chars xloc yloc zloc scale)
    (.end3DRendering text-renderer)))

(defn layout-level [level]
  (let [width (:width level)
        height (:height level)
        offset-x (* (- 1 width) cell-size 0.5)
        offset-y (* (- 1 height) cell-size 0.5)]
    (for [i (range width)
          j (range height)
          :let [x (+ offset-x (* i cell-size))
                y (- (+ offset-y (* j cell-size)))
                idx [i j]]]
      {:x x :y y :idx idx :image ((:blocks level) idx)})))

(defn render-cells [^GL2 gl cells active-brush cell-size-half]
  #_(gl/with-enabled gl [shader]
     (doseq [cell cells]
       (let [color (if (= active-brush (:image cell)) (get-selected-image-color (:image cell)) (get-image-color (:image cell)))]
         (shader/set-uniform shader gl "color" (float-array color))
         (render-quad gl (:x cell) (:y cell) (/ cell-size-half 1.1))))))

(defn render-palette [ctx ^GL2 gl text-renderer gpu-texture vertex-binding layout]
  (doseq [group layout]
    (render-text ctx gl text-renderer (:name group) (- (:x group) palette-cell-size-half) (+ (* 0.25 group-spacing) (- palette-cell-size-half (:y group))) 0 1))
  (let [cell-count (reduce (fn [v0 v1] (+ v0 (count (:cells v1)))) 0 layout)]
   (gl/with-enabled gl [gpu-texture shader vertex-binding]
    (shader/set-uniform shader gl "texture" (texture/texture-unit-index gl gpu-texture))
    (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (* 6 cell-count)))))

(defn render-level [^GL2 gl level gpu-texture vertex-binding layout]
  (let [cell-count (count layout)]
   (gl/with-enabled gl [gpu-texture shader vertex-binding]
    (shader/set-uniform shader gl "texture" (texture/texture-unit-index gl gpu-texture))
    (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (* 6 cell-count)))))

(defn anim-uvs [textureset id]
  (let [anim (first (filter (fn [anim] (= id (:id anim))) (:animations textureset)))
        frame (first (:frames anim))]
    (if frame
      (:tex-coords frame)
      [[0 0] [0 0]])
    ))

(defn flip [v0 v1]
  [v1 v0])

(defn gen-quad [textureset cell size-half flip-vert]
  (let [x (:x cell)
        y (:y cell)
        x0 (- x size-half)
        y0 (- y size-half)
        x1 (+ x size-half)
        y1 (+ y size-half)
        [[u0 v0] [u1 v1]] (anim-uvs textureset (:image cell))
        [v0 v1] (if flip-vert (flip v0 v1) [v0 v1])]
    [[x0 y0 0 1 u0 v0]
     [x1 y0 0 1 u1 v0]
     [x0 y1 0 1 u0 v1]
     [x1 y0 0 1 u1 v0]
     [x1 y1 0 1 u1 v1]
     [x0 y1 0 1 u0 v1]]
    ))

(defn cells->quads [textureset cells size-half flip-vert]
  (mapcat (fn [cell] (gen-quad textureset cell size-half flip-vert)) cells))

(defn gen-palette-vertex-buffer
  [textureset layout size-half]
  (let [cell-count (reduce (fn [v0 v1] (+ v0 (count (:cells v1)))) 0 layout)
        vbuf  (->texture-vtx (* 6 cell-count))]
    (doseq [group layout]
      (doseq [vertex (cells->quads textureset (:cells group) palette-cell-size-half false)]
        (conj! vbuf vertex)))
    (persistent! vbuf)))

(defn gen-level-vertex-buffer
  [textureset layout size-half]
  (let [cell-count (count layout)
        vbuf  (->texture-vtx (* 6 cell-count))]
    (doseq [vertex (cells->quads textureset layout cell-size-half true)]
      (conj! vbuf vertex))
    (persistent! vbuf)))

(defnk produce-renderable
  [this level gpu-texture texture-packing palette-vertex-binding level-vertex-binding active-brush palette-layout level-layout]
  {pass/overlay [{:world-transform g/Identity4d 
                  :render-fn (fn [ctx gl glu text-renderer]
                               (render-palette ctx gl text-renderer gpu-texture palette-vertex-binding palette-layout))}]
   pass/transparent [{:world-transform g/Identity4d 
                    :render-fn (fn [ctx gl glu text-renderer] 
                                 (render-level gl level gpu-texture level-vertex-binding level-layout))}]})

(n/defnode CandyRender
  (input level s/Any)
  (input gpu-texture s/Any)
  (input texture-packing s/Any)
  (input textureset s/Any)
  (input active-brush s/Str)
  (output palette-layout s/Any (fnk [] (layout-palette palette)))
  (output level-layout s/Any (fnk [level] (layout-level level)))
  (output palette-vertex-binding s/Any        (fnk [textureset palette-layout] (vtx/use-with (gen-palette-vertex-buffer textureset palette-layout palette-cell-size-half) shader)))
  (output level-vertex-binding s/Any        (fnk [textureset level-layout] (vtx/use-with (gen-level-vertex-buffer textureset level-layout cell-size-half) shader)))
  (output renderable t/RenderData produce-renderable))

(def prev-move-event (atom {:x 0 :y 0}))

(n/defnode CandyNode
  (inherits n/OutlineNode)
  (input camera s/Any)
  (property level s/Any (visible false))
  (property width s/Int)
  (property height s/Int)
  #_DEMO
  #_(property game-mode s/Str (default "Collect Items"))
  (property active-brush s/Str (default "red_candy"))  
  (output aabb AABB (fnk [width height]
                         (let [half-width (* 0.5 cell-size width)
                               half-height (* 0.5 cell-size height)]
                           (t/->AABB (Point3d. (- half-width) (- half-height) 0)
                                     (Point3d. half-width half-height 0)))))
  
  (on :mouse-move
      (reset! prev-move-event event))
  (on :mouse-down
      (let [camera (first (n/get-node-inputs self :camera))
            pos {:x (:x event) :y (:y event)}
            world-pos-v4 (camera-unproject camera (:x event) (:y event) 0)
            world-pos {:x (.x world-pos-v4) :y (.y world-pos-v4)} 
            level (:level self)
            palette-cells (mapcat :cells (layout-palette palette))
            palette-hit (some #(hit? % pos palette-cell-size-half) palette-cells)
            level-cells (layout-level level)
            level-hit (some #(hit? % world-pos cell-size-half) level-cells)]
        (when palette-hit
          (ds/set-property self :active-brush (:image palette-hit)))
        (when level-hit
          (ds/set-property self :level (assoc-in level [:blocks (:idx level-hit)] (:active-brush self))))))
  (on :load
      (let [project (:project event)
            level (load-level (slurp (:filename self)))]
        (ds/set-property self :width (:width level))
        (ds/set-property self :height (:height level))
        (ds/set-property self :level level))))

(defn broadcast-event [this event]
  (let [[controllers] (n/get-node-inputs this :controllers)]
    (doseq [controller controllers]
      (t/process-one-event controller event))))

(n/defnode BroadcastController
  (input controllers [s/Any])
  (on :mouse-down (broadcast-event self event))
  (on :mouse-up (broadcast-event self event))
  (on :mouse-double-click (broadcast-event self event))
  (on :mouse-enter (broadcast-event self event))
  (on :mouse-exit (broadcast-event self event))
  (on :mouse-hover (broadcast-event self event))
  (on :mouse-move (broadcast-event self event))
  (on :mouse-wheel (broadcast-event self event))
  (on :key-down (broadcast-event self event))
  (on :key-up (broadcast-event self event)))

(defn on-edit
  [project-node editor-site candy-node]
  (let [editor (n/construct ed/SceneEditor :name "editor")]
    (ds/in (ds/add editor)
      (let [candy-render   (ds/add (n/construct CandyRender))
            controller     (ds/add (n/construct BroadcastController))
            background     (ds/add (n/construct background/Gradient))
            grid           (ds/add (n/construct grid/Grid))
            camera         (ds/add (n/construct CameraController :camera (make-camera :orthographic)))
            atlas-node      (t/lookup project-node "/candy/candy.atlas")]
        (ds/connect camera         :camera      grid           :camera)
        (ds/connect camera         :camera      editor         :view-camera)
        (ds/connect camera         :camera      candy-node     :camera)
        (ds/connect camera         :self        controller     :controllers)
        (ds/connect candy-node     :self        controller     :controllers)
        (ds/connect controller     :self        editor         :controller)
        (ds/connect background     :renderable  editor         :renderables)
        #_DEMO
        #_(ds/connect grid           :renderable  editor         :renderables)
        (ds/connect candy-render   :renderable  editor         :renderables)
        (ds/connect candy-node     :level       candy-render   :level)
        (ds/connect candy-node     :active-brush candy-render   :active-brush)
        (ds/connect candy-node     :aabb editor   :aabb)
        (ds/connect atlas-node     :gpu-texture  candy-render   :gpu-texture)
        (ds/connect atlas-node     :texture-packing  candy-render   :texture-packing)
        (ds/connect atlas-node     :textureset  candy-render   :textureset))
      editor)))

(when (ds/in-transaction?)
  (p/register-editor "level" #'on-edit)
  (p/register-node-type "level" CandyNode))
