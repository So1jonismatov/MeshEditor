# PBR Architecture & Implementation Status

## Overview
This document tracks the Physically Based Rendering (PBR) pipeline in **MeshEditor**, its alignment with the official **Khronos Group glTF 2.0 Specification** (Core Metallic-Roughness Model & Extensions), the modular shader architecture, and future enhancement opportunities.

---

## 1. Modular Shader Architecture

To ensure readability, maintainability, and clean isolation of concerns, the PBR shader pipeline is divided into dedicated, modular chunks located under [`GLRenderSystem/shaders/`](file:///C:/Users/soismatov/Desktop/LABS/FromTriangleToScene6.0Q226Akymenko/GLRenderSystem/shaders):

| File | Purpose / Responsibilities |
| :--- | :--- |
| [`pbr_brdf.glsl`](file:///C:/Users/soismatov/Desktop/LABS/FromTriangleToScene6.0Q226Akymenko/GLRenderSystem/shaders/pbr_brdf.glsl) | **Core BRDF Math**: Trowbridge-Reitz GGX NDF, Khronos Smith Joint Height-Correlated Visibility ($V$), Schlick Fresnel, Charlie sheen NDF, Ashikhmin visibility, Heitz anisotropic GGX, and Fraunhofer thin-film iridescence. |
| [`pbr_direct.glsl`](file:///C:/Users/soismatov/Desktop/LABS/FromTriangleToScene6.0Q226Akymenko/GLRenderSystem/shaders/pbr_direct.glsl) | **Direct Lighting Stage**: Evaluates directional and point lights, energy conservation ($k_D = (1 - k_S)(1 - m)$), anisotropic/isotropic specular, fabric sheen lobe, clearcoat lobe, and diffuse transmission back-scatter. |
| [`pbr_ibl.glsl`](file:///C:/Users/soismatov/Desktop/LABS/FromTriangleToScene6.0Q226Akymenko/GLRenderSystem/shaders/pbr_ibl.glsl) | **Ambient Environment & IBL Stage**: Split-sum specular reflection, diffuse irradiance, Fdez-Agüera (2020) multi-scattering energy compensation, sheen IBL, clearcoat IBL, and volumetric Beer-Lambert absorption. |
| [`pbr_tonemap.glsl`](file:///C:/Users/soismatov/Desktop/LABS/FromTriangleToScene6.0Q226Akymenko/GLRenderSystem/shaders/pbr_tonemap.glsl) | **Color Grading & Tone Mapping**: Khronos PBR Neutral (2024 standard for true-to-life CAD/eCommerce baseColor reproduction) and ACES Filmic (cinematic S-curve). |
| [`pbr_uniforms.glsl`](file:///C:/Users/soismatov/Desktop/LABS/FromTriangleToScene6.0Q226Akymenko/GLRenderSystem/shaders/pbr_uniforms.glsl) | Uniform declarations and texture samplers for all supported glTF material extensions. |
| [`pbr_lighting.glsl`](file:///C:/Users/soismatov/Desktop/LABS/FromTriangleToScene6.0Q226Akymenko/GLRenderSystem/shaders/pbr_lighting.glsl) | **Pipeline Coordinator**: Unpacks material factors, handles sRGB decoding, computes normal-incidence $F_0$, and invokes direct lighting and IBL passes. |
| [`mesh_fragment.glsl`](file:///C:/Users/soismatov/Desktop/LABS/FromTriangleToScene6.0Q226Akymenko/GLRenderSystem/shaders/mesh_fragment.glsl) | Fragment shader for 3D meshes: evaluates baseColor factor * texture * vertex color, alpha masking, double-sided normal flipping, and wireframe overlay. |
| [`vertex.glsl`](file:///C:/Users/soismatov/Desktop/LABS/FromTriangleToScene6.0Q226Akymenko/GLRenderSystem/shaders/vertex.glsl) & [`geometry.glsl`](file:///C:/Users/soismatov/Desktop/LABS/FromTriangleToScene6.0Q226Akymenko/GLRenderSystem/shaders/geometry.glsl) | Forwards vertex position, normal, color, UV, and authored vertex tangent (`aTangent` / `fTangent`) through the pipeline. |
| [`postprocess_frag.glsl`](file:///C:/Users/soismatov/Desktop/LABS/FromTriangleToScene6.0Q226Akymenko/GLRenderSystem/shaders/postprocess_frag.glsl) | HDR post-processing pass combining bloom and Khronos PBR Neutral tone mapping with gamma correction. |

---

## 2. What Has Been Completed & Verified

### A. Mathematical Spec Alignment (Khronos glTF 2.0 Appendix B)
- [x] **Smith Joint Height-Correlated Visibility ($V_{\text{SmithJointGGX}}$)**:
  $$V(n \cdot l, n \cdot v, \alpha) = \frac{0.5}{n \cdot l \sqrt{(n \cdot v)^2(1 - \alpha^2) + \alpha^2} + n \cdot v \sqrt{(n \cdot l)^2(1 - \alpha^2) + \alpha^2}}$$
  Replaced separable Schlick-GGX. Eliminates grazing-angle over-darkening and removes the unstable $4(N \cdot L)(N \cdot V)$ division in the denominator.
- [x] **BaseColor Multiplier Alignment**:
  $$\mathbf{c}_{\text{base}} = \mathbf{c}_{\text{texture}} \times \mathbf{c}_{\text{factor}} \times \mathbf{c}_{\text{vertexColor}}$$
  Ensures material tinting (`material.diffuse` / `baseColorFactor`) and vertex colors are properly applied when textures are present.
- [x] **Alpha Masking Mode (`MASK`)**:
  Implemented in [`mesh_fragment.glsl`](file:///C:/Users/soismatov/Desktop/LABS/FromTriangleToScene6.0Q226Akymenko/GLRenderSystem/shaders/mesh_fragment.glsl) using `discard` when `alpha < uAlphaCutoff`. Masked cutouts, foliage, and fences now render transparent instead of solid black quads.
- [x] **Double-Sided Geometry (`doubleSided`)**:
  Implemented normal flipping ($N = -N$ when $\text{dot}(N, V) < 0$) for double-sided materials, preventing back-faces from rendering pitch black.
- [x] **sRGB Texture Linearization**:
  Decodes `emissiveTexture`, `sheenColorTexture`, and `specularColorTexture` from sRGB space to Linear space ($c^{2.2}$) before lighting evaluation.
- [x] **Authored Tangent Pass-Through**:
  Passed `aTangent` (vec4) from vertex buffer through [`vertex.glsl`](file:///C:/Users/soismatov/Desktop/LABS/FromTriangleToScene6.0Q226Akymenko/GLRenderSystem/shaders/vertex.glsl) and [`geometry.glsl`](file:///C:/Users/soismatov/Desktop/LABS/FromTriangleToScene6.0Q226Akymenko/GLRenderSystem/shaders/geometry.glsl) to [`mesh_fragment.glsl`](file:///C:/Users/soismatov/Desktop/LABS/FromTriangleToScene6.0Q226Akymenko/GLRenderSystem/shaders/mesh_fragment.glsl). Eliminates screen-space derivative seams on mirrored UV boundaries.
- [x] **Anisotropy Rotation**:
  Rotates the tangent frame by `uAnisotropyRotation` in [`pbr_direct.glsl`](file:///C:/Users/soismatov/Desktop/LABS/FromTriangleToScene6.0Q226Akymenko/GLRenderSystem/shaders/pbr_direct.glsl).
- [x] **Khronos PBR Neutral Tone Mapping (2024)**:
  Hooked into [`postprocess_frag.glsl`](file:///C:/Users/soismatov/Desktop/LABS/FromTriangleToScene6.0Q226Akymenko/GLRenderSystem/shaders/postprocess_frag.glsl) as the default PBR tonemapper to guarantee exact albedo chromaticity for CAD and product rendering.

---

## 3. Supported Khronos glTF Material Extensions

| Extension | Implementation Details | Status |
| :--- | :--- | :--- |
| `KHR_materials_unlit` | Bypasses shading to display unlit flat base color. | Fully Supported |
| `KHR_materials_clearcoat` | Secondary GGX lobe with clearcoat roughness & base layer attenuation. | Fully Supported |
| `KHR_materials_sheen` | Charlie microfacet NDF + Ashikhmin shadow-masking for cloth/velvet. | Fully Supported |
| `KHR_materials_anisotropy` | Directional microfacet elongation with tangent frame rotation. | Fully Supported |
| `KHR_materials_iridescence` | Thin-film interference using Fraunhofer spectral wavelengths. | Fully Supported |
| `KHR_materials_transmission` | Optical refraction using Snell's law and screen-space framebuffer sampling. | Fully Supported |
| `KHR_materials_volume` | Volumetric absorption using Beer-Lambert law ($\exp(-\sigma \cdot d)$). | Fully Supported |
| `KHR_materials_ior` | Index of refraction overriding default 1.5 ($F_0 = 0.04$). | Fully Supported |
| `KHR_materials_specular` | Dielectric specular tint and reflectance scaling. | Fully Supported |
| `KHR_materials_emissive_strength` | HDR emission multiplier. | Fully Supported |
| `KHR_materials_diffuse_transmission` | Subsurface light penetration through back-facing thin geometry. | Fully Supported |

---

## 4. What Needs to Be Done (Future Enhancements)

The following items are optional advanced features that can be added in future iterations:

1. **HDRI Cubemap Environment Loading**:
   - *Current*: Procedural sky gradient with analytical sun highlight ([`sampleEnvironment`](file:///C:/Users/soismatov/Desktop/LABS/FromTriangleToScene6.0Q226Akymenko/GLRenderSystem/shaders/mesh_fragment.glsl#L88-L119)).
   - *Future*: Support loading `.hdr` / `.exr` environment probes, generating prefiltered specular cubemaps (with roughness mips) and irradiance maps at runtime.
2. **Precomputed 2D BRDF LUT Texture**:
   - *Current*: Analytical polynomial approximation ([`EnvBRDFApprox`](file:///C:/Users/soismatov/Desktop/LABS/FromTriangleToScene6.0Q226Akymenko/GLRenderSystem/shaders/pbr_brdf.glsl#L97-L105)).
   - *Future*: Precompute a 512×512 16-bit float 2D texture encoding $R = \int_{\Omega} f_r \cos\theta d\omega_i$ to reach mathematical parity with offline raytracers.
3. **`KHR_texture_transform` Support**:
   - Support per-texture UV scale, offset, and rotation properties from glTF materials.
