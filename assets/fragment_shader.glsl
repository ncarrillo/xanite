#version 300 es
precision highp float;

in vec2 texCoord;
in vec3 fragNormal;
in vec3 fragPosition;
in vec3 fragLightDir;

out vec4 fragColor;

uniform sampler2D textureSampler;

uniform vec3 lightColor;
uniform vec3 ambientLight;
uniform vec3 diffuseLight;
uniform vec3 specularLight;
uniform float shininess;

uniform float alphaThreshold;
uniform float gammaCorrection; 
uniform vec3 cameraPosition;     


float computeDiffuse(vec3 normal, vec3 lightDir) {
    return max(dot(normalize(normal), normalize(lightDir)), 0.0);
}

float computeSpecular(vec3 normal, vec3 lightDir, vec3 viewDir, float shininess) {
    vec3 reflectDir = reflect(-normalize(lightDir), normalize(normal));
    return pow(max(dot(viewDir, reflectDir), 0.0), shininess);
}

vec3 applyGammaCorrection(vec3 color, float gamma) {
    return pow(color, vec3(1.0 / gamma));
}

void main() {
    vec4 textureColor = texture(textureSampler, texCoord);

      
    float diff = computeDiffuse(fragNormal, fragLightDir);

    vec3 viewDir = normalize(cameraPosition - fragPosition);
    float spec = computeSpecular(fragNormal, fragLightDir, viewDir, shininess);


    vec3 finalColor = ambientLight + diffuseLight * diff + specularLight * spec;

    
    vec3 resultColor = textureColor.rgb * finalColor;
    
    resultColor = applyGammaCorrection(resultColor, gammaCorrection);

    
    float alpha = textureColor.a;
    if (alpha < alphaThreshold) {
        discard;
    }

    fragColor = vec4(resultColor, alpha);
}