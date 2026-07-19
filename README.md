# segagame
a sample game that can be used as a template

<!-- MOTIVATIONAPP:START -->
## MotivationApp Engineering Summary

segagame remains a documentation-only planning stub for a proposed Sega Mega Drive dodgeball game styled after FIFA 94; no ROM, source code, build artifacts, or runnable output exists. The only content is a one-line README.md ('a sample game that can be used as a template') and docs/planning.md, which specifies a scene-driven architecture, TEST/PLAY startup modes, and a 12-row status table where every component (game loop, 3 scenes, input manager, player/ball entities, collision manager, AI manager, graphics, music/SFX, test framework) is marked 'Not started'. Since the prior review on this same date, no new commits or file additions have occurred; the repository is unchanged and remains pre-implementation with zero verifiable functionality.

- **Current priority:** Run 'git add docs/planning.md README.md && git commit' to bring the untracked docs/planning.md and modified README.md into version control, exactly as recommended in the prior review pass on this same date; verify with 'git status' showing a clean working tree — this action still has not been taken.
- **Larger goal:** Stand up the first executable slice: create /src with a minimal TEST-mode entry point and one isolated test per docs/planning.md's TDD philosophy, first resolving the open question of how tests are stored/selected in TEST mode; done when a TEST-mode build compiles, runs, and reports at least one passing modular test, closing the current zero-source, zero-test gap that has persisted unchanged across this and the prior review.
- **Subprojects/components analysed:** 0
- **Related projects:** No local project relationships detected.
- **Full reviewed summary:** [docs/MOTIVATIONAPP_SUMMARY.md](docs/MOTIVATIONAPP_SUMMARY.md)
- **Last reviewed:** 2026-07-19T18:29:48Z

_This section is maintained automatically by the MotivationApp Repo Insight flow._
<!-- MOTIVATIONAPP:END -->
