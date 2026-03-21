# Foundation
This engine is used as the basis for research and general education in graphics programming. Main feature of this engine is virtualized geometry heavily inspired by Unreal Engines Nanite system.

# Screenshots

![](https://github.com/lukasino1214/foundation/blob/rewrite/media/bistro_main.png)

![](https://github.com/lukasino1214/foundation/blob/rewrite/media/intel_sponza.png)

![](https://github.com/lukasino1214/foundation/blob/rewrite/media/many_bistros.png)

# Features
## Async Asset Loading
The asset loader is fully asynchronous, built on top of a custom thread pool. Loading state is tracked through a manifest system — a strictly append-only collection of descriptors for meshes, textures, and associated metadata. Each descriptor is populated on first load and persists for the duration of the application's runtime. Currently, only the glTF format is supported.

Once a manifest entry is created, the thread pool receives one job per texture and one per mesh. Each job independently handles the full pipeline for its resource: loading from disk, processing, and uploading to the GPU. On completion, the manifest is updated with the resulting runtime data — pointers to the relevant GPU buffers and textures. The rendering system polls this data, and as soon as a resource is resident on the GPU, the mesh is drawn and textured accordingly.

### Texture Processing
Before uploading to the GPU, raw texture data is compressed. Albedo and normal textures require additional processing beyond simple compression.

For alpha-tested materials, alpha to coverage is applied to prevent artifacts such as fadding out in distance. Normal textures are packed from three channels down to two using the technique used by Killzone 2, then compressed with BC5 and the same format used for roughness/metallic textures. Albedo textures are compressed to BC1 for opaque materials and BC3 for alpha-tested ones. All block compression is handled by bc7enc_rdo.

### Mesh Processing

Before rendering, every mesh must be preprocessed into a meshlet representation. This involves several stages, most of which are handled by the meshoptimizer library.

Vertex and index optimization is the first step. The vertex cache, overdraw, and vertex fetch are all optimized to improve GPU efficiency before any further processing takes place.

Meshlet generation comes next. Since most model formats only store raw vertices and indices, meshlets must be generated at load time. A meshlet, sometimes called a cluster, is a small group of triangles organized to maximize spatial locality and cache efficiency. meshoptimizer handles the generation, grouping nearby triangles into compact clusters that the GPU can process efficiently.

Culling data is then generated for each meshlet both a bounding sphere, provided directly by meshoptimizer, and an AABB, which is computed manually by iterating over each meshlet's vertices.

Hierarchical LOD generation is the final and most involved stage. LODs are generated automatically through a iterative algorithm that runs until only a single meshlet remains:

1. Group nearby meshlets. For each meshlet, its neighbors are identified by finding shared vertices. This connectivity data is expressed as a graph in CSR format and passed to the METIS library, which partitions it into groups of spatially coherent meshlets.

2. Lock group borders. To prevent cracks forming between groups during simplification, boundary vertices and those shared between two or more groups are locked and excluded from simplification. A vertex is marked locked if it belongs to more than one group.

3. Simplify each group. A local index buffer is assembled for the group and passed to meshoptimizer's meshopt_simplifyWithAttributes, along with vertex positions, lock flags, and attribute weights. If the resulting simplification error exceeds a set threshold, the result is discarded and the group is left unmodified.

4. Generate new meshlets from the simplified geometry, identical to the process used for the original mesh.

5. Generate culling data for the new meshlets again using the same approach as before. Additionally, a group-level bounding sphere is computed, with its center as the weighted average of constituent meshlet centers and its radius covering the furthest extent of any meshlet in the group. Each meshlet in the group is then assigned this bounding sphere alongside the maximum simplification error accumulated up to that LOD level. These two values are what drive LOD selection at runtime.

This process repeats, each iteration working on the meshlets produced by the previous one, until the mesh has been reduced to a single meshlet.

Once all preprocessing is complete, the mesh data is packed into a single contiguous GPU buffer. A Mesh struct holds all associated metadata (vertex count, AABB...), and pointers into the sections of the buffer containing the meshlet descriptors, vertex attributes, indices, per-meshlet AABBs, and simplification errors. This layout keeps everything needed for both rendering and culling in one place, accessible with minimal indirection.

References:
 - https://advances.realtimerendering.com/s2021/Karis_Nanite_SIGGRAPH_Advances_2021_final.pdf

## Visibility Buffer Rendering
![](https://github.com/lukasino1214/foundation/blob/rewrite/media/visibility_buffer.png)

Foundation uses a GPU-driven, fully bindless rendering pipeline. All geometry mesh, then cluster, then triangle culled and finally drawn to visibility buffer (triangle ID + depth).

### GPU Scene
GPU-driven rendering requires scene data to be prepared on the GPU side. Rather than coupling the CPU and GPU scenes together, Foundation maintains them as strictly separate representations.

The GPU scene stores only renderer-relevant data, flattened into a series of buffers. When an entity is assigned a mesh or a light, it receives a handle from the GPU scene rather than sharing any CPU-side state directly.

Each entity may have a single MeshGroup, which can be shared across multiple entities. A MeshGroup is a collection of meshes, where each mesh may have different materials and drawing requirements.

### GPU Culling
An illustration of the culling. The first image shows the view of the main camera. The second image shadows the scene culled from the an observer camera point of view.

![](https://github.com/lukasino1214/foundation/blob/rewrite/media/culling_main.png)

![](https://github.com/lukasino1214/foundation/blob/rewrite/media/culling_view.png)

All culling is done on the GPU. Foundation culls against frustum, HiZ(Hierarchical Z buffer), for meshes and meshlets. Aditionally for meshlets I am deciding which LOD level I am going to render. For triangles, Foundation addionally culls backfaces and if triangle is small enough that it misses all rasterization samples gets culled too. The triangle culling happens in mesh shaders.

References:
 - https://zeux.io/2023/04/28/triangle-backface-culling/

### Draw pass
Foundation draws all geometry "in one draw call". This means extremely low CPU overhead independent of the amount geometry on the screen. This way of drawing geometry also reduces GPU state switches.

Foundation seperates mesh to three categorizes that are opaque, alpha tested (masked) and transparent (blended). This separation is carried further during culling, where each meshlet is additionally classified for either hardware or software rasterization. This currently only applies to the opaque meshlets. All of this is done to minimize ammount of GPU state switches and maximizing performance.

Both opaque and alpha-tested meshlets render a visibility buffer, storing a 32-bit value packed as a 26-bit meshlet instance index and a 6-bit triangle index, alongside depth. Opaque meshlets are additionally software rasterized via a compute shader, where visibility and depth are written atomically to an R64 image using InterlockedAdd to safely handle concurrent writes.

Transparent meshlets follow a different path entirely, rendered in a Forward+ fashion rather than through the visibility buffer. This is necessary because transparency resolution requires both color and transmission data. Weighted Blended Order-Independent Transparency (WBOIT) is used to composite the final result.

### Visibility Buffer
The final step of my geometry pipeline is resolving visibility buffer. Just using the visibility buffer, I can fully reconstruct all triangle information in deferred manner. The only information needed is visibility ID (26-bit meshlet instance index and a 6-bit triangle index). Making overdraw incredibly cheap because fragment does virtually nothing. This makes depth prepass not necessary anymore.

To resolve the visibility buffer:
  1. Foundation fetches visibility ID
  2. splits it into meshlet instance ID and meshlet triangle ID
  3. fetches the meshlet instance, this gives us mesh instance ID
  4. fetches the meshes meta data, which gives us a pointer to the meshlet, vertex and triangle data
  5. fetches the tree vertices of the meshlets triangle
  6. transforms them and their attributes
  7. calculates barycentrics based on the vertex positions and pixel primary ray
  8. interpolates the vertices using the barycentrics

The visibility buffer gets decoded twice. Once, to generate slim G-Buffer that is require for deferred effects such as SSAO and Bloom. The second decoding is when I am shading the whole visibility buffer.

References:
 - http://filmicworlds.com/blog/visibility-buffer-rendering-with-material-graphs/


## Post Processing

### SSAO
Foundation currently implements this as a simple SSAO-style screen-space ambient occlusion pass rather than a full deferred AO solution. After the visibility buffer is resolved into a slim G-Buffer, the renderer stores world-space normals in a packed normal target and uses the main depth buffer to reconstruct view-space positions.

For each pixel, the SSAO compute pass builds a small random tangent basis from the decoded normal, rotates a fixed 26-sample hemisphere kernel into view space, projects those samples back into screen space, and compares them against the depth buffer. If nearby geometry is found in front of the expected sample point, that direction is considered occluded. The final occlusion term is written into a single-channel image.

This occlusion is only applied to indirect lighting during the visibility-buffer shading pass. In other words, it darkens ambient sky/IBL contribution and contact areas without suppressing direct sun or local light contributions.

### Bloom
Bloom in Foundation is seeded in two places. During G-Buffer generation, emissive material output is written into a dedicated HDR bloom image. After the main lighting pass, a threshold step adds any HDR overflow above `1.0` from the lit render image into that same bloom source, so both emissive surfaces and very bright highlights contribute.

The bloom pyramid is built in compute. The renderer downsamples into a mip chain, using a 13-tap anti-flicker filter on the first downsample and a weighted 13-tap filter for the remaining levels. It then upsamples back through the chain with a 9-tap blur and blends each level with the next using a fixed internal blend factor.

The result is not added back at full strength. The final pass linearly blends between the original HDR image and `original + bloom`, with a conservative factor of `0.04`, so bloom stays subtle and mostly reinforces strong emissive or overbright areas.

### Auto Exposure
Auto exposure is histogram-based. After bloom is applied, the renderer scans the HDR render image and bins per-pixel luminance into a 256-bin log-luminance histogram using perceived luminance weights. The histogram range is configurable and currently spans from `log2(1 / 4096)` to `log2(4096)`.

Rather than averaging the full histogram, the code rejects a large low-end portion and the brightest outliers. Right now it uses only the `60%` to `95%` histogram interval, which stabilizes exposure by ignoring very dark pixels and extreme highlights. From that trimmed range it computes a target EV value.

Exposure does not snap instantly. The previous EV is stored in a persistent buffer, and each frame moves toward the target using an exponential adaptation term controlled by `luminance_adaption_tau`. The adapted EV is then converted into a scalar exposure using the configured calibration, sensor sensitivity, and exposure bias.

### Tone Mapping
Tone mapping happens in the final swapchain write pass. The HDR render image is first multiplied by the auto-exposure value stored in the exposure buffer, then passed through an AgX-style tone mapper.

The implementation converts linear sRGB into Rec.2020, applies the AgX inset transform, encodes the result into log space, runs a contrast approximation, applies a custom AgX look adjustment, and transforms the result back to linear sRGB. After that, values are clamped to display range and encoded with the standard sRGB transfer function before writing to the swapchain.

In practice, this gives Foundation a filmic display transform with smoother highlight rolloff than a simple clamp or Reinhard-style curve, while keeping the final image in a standard sRGB presentation format.
