#version 330 core

layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
} fs_in;

struct Light {
    vec3 Position;
    vec3 Color;
};

uniform Light lights[4];
uniform sampler2D diffuseTexture;
uniform vec3 viewPos;
uniform vec3 enemyColor; 
uniform float hitFlash; 
uniform bool hasTexture;
uniform bool useTintOnly;

void main()
{           
    // Sample the texture color
    vec3 color = texture(diffuseTexture, fs_in.TexCoords).rgb;
    // Multiply by the enemy's color
    color *= enemyColor; // Apply the color change here

    // Normalize the normal vector
    vec3 normal = normalize(fs_in.Normal);
    
    // Ambient lighting (using 0 for now, adjust if needed)
    vec3 ambient = 0.0 * color;
    
    // Lighting calculations
    vec3 lighting = vec3(0.0);
    vec3 viewDir = normalize(viewPos - fs_in.FragPos);
    
    for(int i = 0; i < 4; i++)
    {
        // Diffuse lighting
        vec3 lightDir = normalize(lights[i].Position - fs_in.FragPos);
        float diff = max(dot(lightDir, normal), 0.0);
        vec3 result = lights[i].Color * diff * color;  // Apply color to lighting result
        
        // Attenuation based on distance (quadratic falloff)
        float distance = length(fs_in.FragPos - lights[i].Position);
        result *= 1.0 / (distance * distance);
 
        lighting += result;
    }
    
    // Final result of ambient + lighting
    vec3 result = ambient + lighting;
    // Emissive flash so it gets bright (bloom will catch this)
    result += enemyColor * hitFlash;
    
    if (useTintOnly) {
        result = enemyColor;
    }
    
    // Brightness check for bloom effect
    float brightness = dot(result, vec3(0.2126, 0.7152, 0.0722));
    if(brightness > 1.0)
        BrightColor = vec4(result, 1.0);
    else
        BrightColor = vec4(0.0, 0.0, 0.0, 1.0);

   
    // Final color output
    FragColor = vec4(result, 1.0);
}
