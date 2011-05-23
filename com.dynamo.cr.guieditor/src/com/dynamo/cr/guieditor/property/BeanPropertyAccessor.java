package com.dynamo.cr.guieditor.property;

import java.beans.BeanInfo;
import java.beans.IntrospectionException;
import java.beans.Introspector;
import java.beans.PropertyDescriptor;
import java.lang.reflect.InvocationTargetException;

public class BeanPropertyAccessor implements IPropertyAccessor<Object, IPropertyObjectWorld> {

    private PropertyDescriptor propertyDescriptor;

    private void init(Object obj, String property) {

        if (propertyDescriptor != null)
            return;

        try {
            BeanInfo beanInfo = Introspector.getBeanInfo(obj.getClass());
            PropertyDescriptor[] propertyDescriptors = beanInfo.getPropertyDescriptors();
            for (PropertyDescriptor propertyDescriptor : propertyDescriptors) {
                if (propertyDescriptor.getName().equals(property)) {
                    this.propertyDescriptor = propertyDescriptor;
                }
            }
        } catch (IntrospectionException e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    public void setValue(Object obj, String property, Object value,
            IPropertyObjectWorld world)
            throws IllegalArgumentException, IllegalAccessException,
            InvocationTargetException {
        init(obj, property);
        propertyDescriptor.getWriteMethod().invoke(obj, value);

    }

    @Override
    public Object getValue(Object obj, String property,
            IPropertyObjectWorld world)
            throws IllegalArgumentException, IllegalAccessException,
            InvocationTargetException {
        init(obj, property);
        return propertyDescriptor.getReadMethod().invoke(obj);
    }
}
