package com.dynamo.cr.parted;

import java.net.URL;

import org.eclipse.core.runtime.FileLocator;
import org.eclipse.jface.resource.ImageDescriptor;
import org.eclipse.jface.resource.ImageRegistry;
import org.osgi.framework.BundleContext;

import com.dynamo.cr.editor.ui.AbstractDefoldPlugin;

/**
 * The activator class controls the plug-in life cycle
 */
public class ParticleEditorPlugin extends AbstractDefoldPlugin {

	// The plug-in ID
	public static final String PLUGIN_ID = "com.dynamo.cr.parted"; //$NON-NLS-1$

    public static final String EMITTER_IMAGE_ID = "EMITTER"; //$NON-NLS-1$

    public static final String PARTED_CONTEXT_ID = "com.dynamo.cr.parted.contexts.partEditor";

	// The shared instance
	private static ParticleEditorPlugin plugin;

	/**
	 * The constructor
	 */
	public ParticleEditorPlugin() {
	}

	public void start(BundleContext context) throws Exception {
		super.start(context);

		URL bundleUrl;
		if (System.getProperty("os.name").toLowerCase().indexOf("mac") != -1) {
		    // The editor is 64-bit only on Mac OS X and shared libraries are
		    // loaded from platform directory
	        bundleUrl = getBundle().getEntry("/DYNAMO_HOME/lib/x86_64-darwin");
		} else {
		    // On other platforms shared libraries are loaded from default location
		    // We should perhaps always use qualifed directories?
            bundleUrl = getBundle().getEntry("/DYNAMO_HOME/lib");
		}

        URL fileUrl = FileLocator.toFileURL(bundleUrl);
        System.setProperty("jna.library.path", fileUrl.getPath());

		plugin = this;
	}

	public void stop(BundleContext context) throws Exception {
		plugin = null;
		super.stop(context);
	}

	@Override
	protected void initializeImageRegistry(ImageRegistry reg) {
	    super.initializeImageRegistry(reg);
        registerImage(reg, EMITTER_IMAGE_ID, "icons/dynamite.png");
        registerImage(reg, "acceleration", "icons/dynamite.png");
	}

    private void registerImage(ImageRegistry registry, String key,
            String fileName) {

        ImageDescriptor id = imageDescriptorFromPlugin(PLUGIN_ID, fileName);
        registry.put(key, id);
    }

	/**
	 * Returns the shared instance
	 *
	 * @return the shared instance
	 */
	public static ParticleEditorPlugin getDefault() {
		return plugin;
	}

}
