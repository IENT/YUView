#version 440

// ============================================================================
// HDR YUV->RGB Fragment Shader - High Performance Version
// ============================================================================
//
// Pipeline for PQ mode:
//   YUV -> BT.2020 R'G'B' -> PQ EOTF -> linear BT.2020
//   -> Linear exposure scaling -> gamut + scRGB scale
//
// Exposure uses a linear luminance multiplier:
//   display_luminance *= userSelectedNits / EOTF_peak_nits
// with logarithmic UI spacing handled in C++.
// Active for PQ and HLG. Linear mode bypasses exposure.
//
// Output: Linear light in scRGB space (1.0 = 80 nits)
// ============================================================================

precision highp float;

layout(location = 0) in vec2 TexCoord;
layout(location = 0) out vec4 FragColor;

layout(std140, binding = 0) uniform UniformBlock {
    mat4 projectionMatrix;
    mat4 textureMatrix;
    mat4 colorMatrix;       // YCbCr -> R'G'B' (BT.2020)
    vec4 offsetVec;         // xyz: Y/U/V offsets
    vec4 hdrParams;         // x: exposureNits, y: displayMax, z: renderMode, w: exposureEnabled
};

layout(binding = 1) uniform sampler2D texY;
layout(binding = 2) uniform sampler2D texU;
layout(binding = 3) uniform sampler2D texV;

// ============================================================================
// Compile-Time Constants
// ============================================================================

// PQ EOTF inverse constants (PQ signal -> linear light)
const float PQ_M1_INV = 6.27739463602;      // 16384/2610
const float PQ_M2_INV = 0.01268331352;      // 4096/322944
const float PQ_C1 = 0.8359375;              // 3424/4096
const float PQ_C2 = 18.8515625;             // 2413/128
const float PQ_C3 = 18.6875;                // 2392/128

// HLG constants
const float HLG_A_INV = 5.59181630973;      // 1/0.17883277
const float HLG_B = 0.28466892;
const float HLG_C = 0.55991073;
const float HLG_GAMMA_M1 = 0.2;             // gamma - 1 for 1000 nits

// BT.2020 luma coefficients
const vec3 BT2020_LUMA = vec3(0.2627, 0.6780, 0.0593);

// ============================================================================
// Pre-merged Gamut Conversion + scRGB Scaling Matrices
// ============================================================================

// PQ mode: BT.2020->sRGB * 125.0 (10000 nits / 80 nits)
const mat3 MAT_BT2020_TO_SCRGB_PQ = mat3(
    207.5625, -15.575, -2.275,
    -73.4625, 141.625, -12.575,
    -9.1,     -1.05,   139.8375
);

// HLG mode: BT.2020->sRGB * 12.5 (1000 nits / 80 nits)
const mat3 MAT_BT2020_TO_SCRGB_HLG = mat3(
    20.75625, -1.5575, -0.2275,
    -7.34625, 14.1625, -1.2575,
    -0.91,    -0.105,  13.98375
);

// Linear mode: BT.2020->sRGB (scale applied separately from uniform)
const mat3 MAT_BT2020_TO_SRGB = mat3(
    1.6605, -0.1246, -0.0182,
   -0.5877,  1.1330, -0.1006,
   -0.0728, -0.0084,  1.1187
);

const int MODE_PQ = 1;
const int MODE_HLG = 2;

// ============================================================================
// Main Fragment Shader
// ============================================================================

void main() {
    // Stage 1: Sample YUV and convert to non-linear RGB (BT.2020)
    vec3 yuv = vec3(
        texture(texY, TexCoord).r,
        texture(texU, TexCoord).r,
        texture(texV, TexCoord).r
    ) - offsetVec.xyz;

    vec3 rgb = mat3(colorMatrix) * yuv;

    // Stage 2: Mode-specific processing (single uniform-based branch)
    int mode = int(hdrParams.z + 0.5);
    float exposureNits = hdrParams.x;
    bool exposureEnabled = hdrParams.w > 0.5 && exposureNits > 0.0;

    rgb = clamp(rgb, 0.0, 1.0);

    if (mode == MODE_PQ) {
        // ====================================================================
        // PQ Mode: EOTF -> linear exposure -> gamut + scRGB scale
        // ====================================================================

        // PQ EOTF: decode PQ signal to linear light [0,1] where 1.0 = 10000 nits
        vec3 Nm2 = pow(max(rgb, vec3(1e-10)), vec3(PQ_M2_INV));
        vec3 num = max(Nm2 - PQ_C1, 0.0);
        vec3 den = max(PQ_C2 - PQ_C3 * Nm2, 1e-6);
        vec3 linear2020 = pow(max(num / den, vec3(1e-10)), vec3(PQ_M1_INV));

        if (exposureEnabled) {
            linear2020 *= exposureNits * 0.0001;
        }

        // Gamut conversion (BT.2020->sRGB) + scRGB scale (*125)
        rgb = MAT_BT2020_TO_SCRGB_PQ * linear2020;
    }
    else if (mode == MODE_HLG) {
        // ====================================================================
        // HLG Mode: OETF^-1 -> OOTF -> linear exposure -> gamut + scRGB scale
        // ====================================================================

        vec3 low = rgb * rgb * 0.333333333;
        vec3 high = (exp((rgb - HLG_C) * HLG_A_INV) + HLG_B) * 0.083333333;
        vec3 t = step(0.5, rgb);
        vec3 scene = mix(low, high, t);

        float Ys = dot(scene, BT2020_LUMA);
        float gain = pow(max(Ys, 1e-6), HLG_GAMMA_M1);
        vec3 linear2020 = scene * gain;

        if (exposureEnabled) {
            linear2020 *= exposureNits * 0.001;
        }

        rgb = MAT_BT2020_TO_SCRGB_HLG * linear2020;
    }
    else {
        // ====================================================================
        // Linear Mode: gamut conversion only (NO tone mapping)
        // ====================================================================
        float scale = hdrParams.y * 0.0125;
        rgb = MAT_BT2020_TO_SRGB * rgb * scale;
    }

    // Stage 3: Output (scRGB can be negative for wide gamut)
    FragColor = vec4(clamp(rgb, vec3(-125.0), vec3(125.0)), 1.0);
}
