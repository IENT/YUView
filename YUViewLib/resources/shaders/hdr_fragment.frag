#version 330 core

// Fragment shader output
out vec4 FragColor;

// Input from vertex shader
in vec2 TexCoord;

// Uniforms
uniform sampler2D videoTexture;  // Video frame texture
uniform int renderMode;          // 0=SDR, 1=BT2020_PQ, 2=BT709_Linear
uniform float hdrExposure;       // HDR exposure adjustment (-10.0 to +10.0)
uniform float hdrGamma;          // Gamma correction (0.1 to 5.0)

// ST.2084 PQ (Perceptual Quantizer) constants for HDR10
// These constants define the PQ transfer function used in HDR10/BT.2020
const float m1 = 2610.0 / 4096.0 / 4.0;        // First exponent
const float m2 = 2523.0 / 4096.0 * 128.0;      // Second exponent  
const float c1 = 3424.0 / 4096.0;              // First coefficient
const float c2 = 2413.0 / 4096.0 * 32.0;       // Second coefficient
const float c3 = 2392.0 / 4096.0 * 32.0;       // Third coefficient

// Color space conversion matrix: Rec.709 to Rec.2020
// Used when converting from standard video content to wide color gamut
const mat3 from709to2020 = mat3(
    0.6274040, 0.3292820, 0.0433136,  // Red channel conversion
    0.0690970, 0.9195400, 0.0113612,  // Green channel conversion  
    0.0163916, 0.0880132, 0.8955950   // Blue channel conversion
);

// Alternative color space conversion matrix: Rec.2020 to Rec.709
// Used when converting from wide color gamut back to standard
const mat3 from2020to709 = mat3(
    1.7166511, -0.3556708, -0.2533663,
    -0.6666844, 1.6164812, 0.0157685,
    0.0176399, -0.0427706, 0.9421031
);

/**
 * Apply ST.2084 PQ (Perceptual Quantizer) transfer function
 * This is the EOTF (Electro-Optical Transfer Function) for HDR10
 * Converts linear light values to PQ-encoded values
 * @param linear Linear RGB values in the range [0, 10000] nits
 * @return PQ-encoded RGB values in the range [0, 1]
 */
vec3 applyPQ(vec3 linear) {
    // CRITICAL FIX: Normalize with range matching our HDR scaling
    // Match the 80 nits scaling used in the main rendering pipeline
    vec3 normalizedLinear = clamp(linear / 100.0, 0.0, 1.0);
    
    // Apply the first power function with exponent m1
    vec3 Lp = pow(max(normalizedLinear, vec3(0.0)), vec3(m1));
    
    // Calculate the numerator: c1 + c2 * Lp
    vec3 numerator = c1 + c2 * Lp;
    
    // Calculate the denominator: 1 + c3 * Lp
    vec3 denominator = 1.0 + c3 * Lp;
    
    // Apply the second power function with exponent m2
    // Ensure we don't divide by zero
    vec3 result = pow(max(numerator / max(denominator, vec3(0.0001)), vec3(0.0)), vec3(m2));
    
    return clamp(result, 0.0, 1.0);
}

/**
 * Apply inverse ST.2084 PQ transfer function  
 * Converts PQ-encoded values back to linear light values
 * @param pqEncoded PQ-encoded RGB values in the range [0, 1]
 * @return Linear RGB values in the range [0, 10000] nits
 */
vec3 applyInversePQ(vec3 pqEncoded) {
    // Clamp input to valid range
    pqEncoded = clamp(pqEncoded, 0.0, 1.0);
    
    // Apply inverse of second power function
    vec3 Lp = pow(pqEncoded, vec3(1.0 / m2));
    
    // Calculate the linear value using inverse PQ formula
    vec3 numerator = max(Lp - c1, vec3(0.0));
    vec3 denominator = c2 - c3 * Lp;
    
    vec3 linear = pow(max(numerator / max(denominator, vec3(0.0001)), vec3(0.0)), vec3(1.0 / m1));
    
    // Scale back to nits range
    return linear * 10000.0;
}

/**
 * Apply exposure adjustment for linear/scRGB mode
 * Exposure is applied as a power of 2 multiplication
 * @param linear Linear RGB values
 * @return Exposure-adjusted RGB values
 */
vec3 applyExposure(vec3 linear) {
    float exposureFactor = pow(2.0, hdrExposure);
    return linear * exposureFactor;
}

/**
 * Apply gamma correction
 * @param color RGB values in any color space
 * @return Gamma-corrected RGB values
 */
vec3 applyGamma(vec3 color) {
    // Prevent negative values and apply gamma curve
    return pow(max(color, vec3(0.0)), vec3(1.0 / max(hdrGamma, 0.1)));
}

/**
 * Tone mapping function for HDR content
 * Simple Reinhard tone mapping to prevent clipping
 * @param hdrColor HDR color values
 * @return Tone-mapped color values
 */
vec3 toneMapReinhard(vec3 hdrColor) {
    return hdrColor / (1.0 + hdrColor);
}

/**
 * ACES filmic tone mapping
 * More sophisticated tone mapping used in film production
 * @param hdrColor HDR color values
 * @return Tone-mapped color values
 */
vec3 toneMapACES(vec3 hdrColor) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    
    return clamp((hdrColor * (a * hdrColor + b)) / (hdrColor * (c * hdrColor + d) + e), 0.0, 1.0);
}

void main()
{
    // Sample the video texture
    vec4 color = texture(videoTexture, TexCoord);
    
    // Process based on the selected render mode
    if (renderMode == 1) {
        // BT2020_PQ mode: HDR10 rendering with PQ transfer function
        
        // CRITICAL FIX: Proper HDR processing to avoid "black mask" effect
        // Input video content is typically gamma-encoded, not linear
        vec3 gammaColor = color.rgb;
        
        // Convert from gamma-encoded to linear space first (approximate sRGB)
        vec3 linearColor = pow(max(gammaColor, vec3(0.0)), vec3(2.2));
        
        // Apply exposure adjustment in linear space
        linearColor = applyExposure(linearColor);
        
        // Convert from Rec.709 to Rec.2020 wide color gamut
        vec3 rec2020Color = from709to2020 * linearColor;
        
        // CRITICAL FIX: More conservative scaling for typical video content
        // Assume input [0,1] in linear space represents 0-80 nits (typical SDR range)
        vec3 hdrLinear = rec2020Color * 80.0; // Conservative HDR range scaling
        
        // Apply PQ transfer function for HDR10 display
        vec3 pqColor = applyPQ(hdrLinear);
        
        // Apply gamma correction if needed
        pqColor = applyGamma(pqColor);
        
        FragColor = vec4(pqColor, color.a);
        
    } else if (renderMode == 2) {
        // BT709_Linear mode: 16-bit scRGB/Linear rendering
        
        // Input is already in linear space, just apply transformations
        vec3 linearColor = color.rgb;
        
        // Apply exposure adjustment
        linearColor = applyExposure(linearColor);
        
        // For scRGB, we can have values > 1.0, so no clamping here
        // Apply gamma correction
        linearColor = applyGamma(linearColor);
        
        // Optional tone mapping for extreme HDR values
        if (any(greaterThan(linearColor, vec3(1.0)))) {
            linearColor = toneMapACES(linearColor);
        }
        
        FragColor = vec4(linearColor, color.a);
        
    } else {
        // SDR mode: Standard 8-bit rendering with optional enhancements
        
        vec3 sdrColor = color.rgb;
        
        // Apply minor exposure adjustment even in SDR mode
        if (abs(hdrExposure) > 0.01) {
            float sdrExposure = clamp(hdrExposure * 0.1, -1.0, 1.0); // Limit SDR exposure
            sdrColor = sdrColor * pow(2.0, sdrExposure);
        }
        
        // Apply gamma correction
        sdrColor = applyGamma(sdrColor);
        
        // Clamp to SDR range
        sdrColor = clamp(sdrColor, 0.0, 1.0);
        
        FragColor = vec4(sdrColor, color.a);
    }
}