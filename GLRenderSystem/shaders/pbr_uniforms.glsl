// PBR Extened Parameters
uniform int uAlphaMode;
uniform float uAlphaCutoff;
uniform int uDoubleSided;
uniform int uUnlit;

uniform float uClearcoatFactor;
uniform float uClearcoatRoughness;

uniform vec3 uSheenColor;
uniform float uSheenRoughness;

uniform float uThickness;
uniform float uAttenuationDistance;
uniform vec3 uAttenuationColor;

uniform float uSpecularFactor;
uniform vec3 uSpecularColor;

uniform float uEmissiveStrength;
uniform float uDispersion;

uniform float uIridescenceFactor;
uniform float uIridescenceIor;
uniform float uIridescenceThicknessMin;
uniform float uIridescenceThicknessMax;

uniform float uAnisotropyStrength;
uniform float uAnisotropyRotation;

uniform float uDiffuseTransmissionFactor;
uniform vec3 uDiffuseTransmissionColor;

// Texture toggles and samplers
uniform int useClearcoatMap;
uniform sampler2D uClearcoatTexture;
uniform int useClearcoatRoughnessMap;
uniform sampler2D uClearcoatRoughnessTexture;

uniform int useSheenColorMap;
uniform sampler2D uSheenColorTexture;
uniform int useSheenRoughnessMap;
uniform sampler2D uSheenRoughnessTexture;

uniform int useTransmissionMap;
uniform sampler2D uTransmissionTexture;

uniform int useThicknessMap;
uniform sampler2D uThicknessTexture;

uniform int useSpecularMap;
uniform sampler2D uSpecularTexture;
uniform int useSpecularColorMap;
uniform sampler2D uSpecularColorTexture;

uniform int useIridescenceMap;
uniform sampler2D uIridescenceTexture;
uniform int useIridescenceThicknessMap;
uniform sampler2D uIridescenceThicknessTexture;

uniform int useAnisotropyMap;
uniform sampler2D uAnisotropyTexture;

uniform int useDiffuseTransmissionMap;
uniform sampler2D uDiffuseTransmissionTexture;
uniform int useDiffuseTransmissionColorMap;
uniform sampler2D uDiffuseTransmissionColorTexture;

uniform sampler2D uOpaqueFramebuffer;
uniform int uUseOpaqueFramebuffer;
uniform vec2 uScreenSize;
uniform int renderWireframe;
