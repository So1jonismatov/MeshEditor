# Workspace Guidelines

This repository follows the geometry engine performance and asynchronous architecture rules defined in [`.agents/rules/geometry-async-guidelines.md`](.agents/rules/geometry-async-guidelines.md).

## Quick Reference
1. **Async Operators**: Always prebuild `PrebuiltBuffers` (`GeometryBuffers::buildBuffersFromHET`) and `PrebuiltOctree` (`FaceOctree::buildOctreeFromHET`) on the worker thread.
2. **Atomic Adoption**: Use `Geometry::adoptPrebuilt(std::move(het), std::move(buffers), std::move(octree), *rs)`. Never call `markDirty()` after adoption.
3. **Zero UI Thread Allocations**: Take snapshots inside the background worker lambda under short locks ($< 0.1\text{ ms}$).
4. **Guaranteed 60 FPS**: `View::update()` uses `std::lock_guard` for continuous rendering without frame drops.
5. **Topology**: Use local 1-ring circulation in $O(\text{valence})$ and stack buffers for candidate edge collapses.
