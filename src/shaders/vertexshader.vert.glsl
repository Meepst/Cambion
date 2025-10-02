#version 450 

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 fragColor;

layout(set = 0, binding = 0) uniform Camera{
    mat4 model;
    mat4 view;
    mat4 proj;
} camera;

void main(){
    gl_Position = camera.model*camera.view*camera.proj*vec4(inPosition, 1.0);
    fragColor = inTexCoord;
}