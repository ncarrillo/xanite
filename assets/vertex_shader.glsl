#version 300 es
precision highp float; // تحديد الدقة

// المدخلات من Java
in vec4 vPosition;
in vec2 vTexCoord;

// المتغيرات التي سيتم إرسالها إلى الـ Fragment Shader
out vec2 texCoord;
out vec3 fragNormal;
out vec3 fragPosition;
out vec3 fragLightDir;

// المصفوفات لتعديل الموقع
uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform vec3 lightPosition; // الموقع الضوئي

void main() {
    // تحويل الرأس باستخدام المصفوفات
    gl_Position = projectionMatrix * modelViewMatrix * vPosition;
    texCoord = vTexCoord;
    
    // إرسال إحداثيات الموضع و الاتجاه إلى الـ Fragment Shader
    fragPosition = vec3(modelViewMatrix * vPosition);
    fragLightDir = normalize(lightPosition - fragPosition);
}