SIMPLE PIANO - synthesized piano instrument (VST3)

Easiest way to build (no Visual Studio needed): GitHub Actions
1. Create a NEW GitHub repository, upload everything in this folder.
2. If the hidden .github folder did not upload, click Add file > Create new file,
   type  .github/workflows/build.yml  as the name, paste in the contents
   of that file from this zip, and commit.
3. Actions tab -> wait for the green check -> download "SimplePiano-windows".
4. Copy "Simple Piano.vst3" (in the VST3 folder) to
   C:\Program Files\Common Files\VST3\
5. FL Studio: Options > Manage plugins > Find installed plugins.
   It appears under Generators (instruments). Add it, then play with
   your keyboard / piano roll.

Or build locally with CMake:
   cmake -B build
   cmake --build build --config Release
