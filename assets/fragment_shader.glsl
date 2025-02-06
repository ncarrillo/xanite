#version 300 es
precision highp float; // تحديد الدقة

// المدخلات من الـ Vertex Shader
in vec2 texCoord;
in vec3 fragNormal;
in vec3 fragPosition;
in vec3 fragLightDir;

// إخراج اللون
out vec4 fragColor;

// الملمس
uniform sampler2D textureSampler;

uniform vec3 lightColor;
uniform vec3 ambientLight;
uniform vec3 diffuseLight;
uniform vec3 specularLight;
uniform float shininess;

uniform float alphaThreshold;

void main() {
    // استخراج اللون من الملمس باستخدام إحداثيات الملمس
    vec4 textureColor = texture(textureSampler, texCoord);

    // حساب الإضاءة المنتشرة (Diffuse)
    float diff = max(dot(normalize(fragNormal), normalize(fragLightDir)), 0.0);

    // حساب الإضاءة المنعكسة (Specular)
    vec3 reflectDir = reflect(-normalize(fragLightDir), normalize(fragNormal));
    vec3 viewDir = normalize(-fragPosition);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);

    // حساب اللون النهائي بناءً على الإضاءة المنتشرة والمنعكسة
    vec3 finalColor = ambientLight + diffuseLight * diff + specularLight * spec;

    // دمج اللون المستخرج من الملمس مع الإضاءة المحسوبة
    vec3 resultColor = textureColor.rgb * finalColor;

    // إضافة الشفافية (Transparency)
    float alpha = textureColor.a;
    if (alpha < alphaThreshold) {
        discard;
    }

    // تعيين اللون النهائي للـ fragment
    fragColor = vec4(resultColor, alpha);
}