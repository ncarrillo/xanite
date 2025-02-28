#version 300 es
precision highp float;

in vec4 vPosition;
in vec2 vTexCoord;
in vec3 vNormal; 

out vec2 texCoord;
out vec3 fragNormal;
out vec3 fragPosition;
out vec3 fragLightDir;

uniform mat4 modelMatrix;      
uniform mat4 viewMatrix;       
uniform mat4 projectionMatrix; 
uniform vec3 lightPosition;  

void main() {
    vec4 worldPosition = modelMatrix * vPosition;
    fragPosition = vec3(worldPosition);
    
    fragNormal = normalize(mat3(modelMatrix) * vNormal);
    
    fragLightDir = normalize(lightPosition - fragPosition);

     
    gl_Position = projectionMatrix * viewMatrix * worldPosition;
    texCoord = vTexCoord;
}