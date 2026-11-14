# GM320 3D model

`gm320-model.glb` is an original, editable glTF 2.0 mesh created for Orbit. It replaces the earlier generic mouse schematic with a reconstruction of the GM320's actual visible design. No vendor CAD, scanned mesh, or model from another mouse application is used.

The top silhouette, eight control locations, asymmetric thumb wing, S-shaped RGB channels and palm-logo placement were traced against the [pinned official product image](GM320-IMAGE.md). Six GM320 listing views, including left, right and oblique views, informed the roof profile, low thumb rest, side-mounted fire switch, two thumb switches, center controls, wheel groove/side-disc geometry, front vents and cable strain relief. Those reference images are retained privately as research artifacts; they are not bundled in the model.

References: [Ant Esports product page](https://antesports.com/products/gm320-8-button-mouse-braided-cable-black) and its linked [GM320 Amazon listing](https://www.amazon.in/Ant-Esports-GM320-Programmable-Comfortable/dp/B08D64C9FN). Current vendor marketing copy has inconsistent button descriptions; the app follows the user's eight-button device and verified protocol.

`gm320-model-spec.json` records the traced image coordinates and reconstruction assumptions. `tools/gm320-model.py` reproduces the GLB on Box using Pillow. The asset uses normalized units with a 2.8-unit body length. Published, verified physical dimensions and a measured scan were not available. Depth, unseen underside, and small manufacturing details remain inferred. This is a detailed visual reconstruction, **not a dimensionally verified CAD replica**. The underside intentionally omits invented sensors, labels and switch commands.

The GLB includes separate materials and IDs for all eight physical controls, smooth vertex normals, emissive RGB surfaces, a wheel and braided cable, plus control-anchor metadata. The asset contains 15,735 vertices and 26,252 triangles. Orbit's native CPU renderer uses perspective-correct interpolation and a depth buffer for materials and picking. Hidden controls remain reachable through their callouts and keyboard shortcuts; their connectors are dashed when occluded.

Original geometry and recipe: copyright 2026 MSedgeMC, MIT. The embedded Ant Esports product-marking textures are extracted from the same official product image and remain their owner's material, outside the MIT grant. See [GM320-IMAGE.md](GM320-IMAGE.md).
