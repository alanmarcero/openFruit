# Screenshots

## OpenVST Manager preview

OpenVST Manager shows `docs/screenshots/preview.png` from `main` (see `screenshotPath` in its catalog), and the GitHub release body and README hero point at the same file. This is the single image to keep current. Refresh it with the main editor capture whenever the UI changes, and commit it before tagging a release:

```sh
cmake -B build -DOF_BUILD_TOOLS=ON && cmake --build build --config Release --target render_ui_snapshot
build/render_ui_snapshot   # writes assets/openFruit-*.png
cp assets/openFruit-screenshot.png docs/screenshots/preview.png
```
