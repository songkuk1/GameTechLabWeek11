# Camera Cut + Fade test

- Scene: Assets/Scenes/SequencerCameraCutTest.scene
- Sequence: Assets/Sequences/SequencerCameraCutTest.sequence
- Duration: 2 seconds / 60 frames (30 fps).
- Existing camera cuts, transforms, lights, and materials are preserved.
- A black FadeTrack is stored in MovieScene.Tracks.

## Fade timing

| Frame | Fade amount | Expected screen |
| --- | --- | --- |
| 0-20 | 0 | Normal view |
| 20-30 | 0 to 1 | Fade to black |
| 30 | 1 | Fully black during the camera switch |
| 30-40 | 1 to 0 | Reveal the second camera |
| 40-60 | 0 | Normal view |

The fade section extends through frame 60 so the final playback evaluation applies amount 0.
CutTest_SequencePlayer has bPlayOnBegin=true and bKeepState=false to test restoring the cube transform and previous ViewTarget at completion. Fade pre-animated-state restoration is not implemented; this test explicitly ends with amount 0.

## Run

1. Reload the sequence asset after changing the file. The current AssetManager returns an already cached instance without re-reading the file; restarting the editor clears this cache.
2. Open SequencerCameraCutTest.scene through the editor scene Open command.
3. Select CutTest_SequencePlayer. In Details, select SequencerCameraCutTest.sequence in Sequence once, because .sequence files are not automatically preloaded for scene references.
4. Start PIE and use the game viewport to verify the fade and camera switch. The editor viewport has no camera-cut/fade preview connection yet.
5. Stop and restart PIE to repeat the test.

JSON structure, scene references, channel defaults, key ordering, fade sample values, and the transparent ending have been checked. Engine execution has not been verified.
