# Branching and release workflow

## Branches
| Branch | Purpose |
|---|---|
| `main` | Released code only. Every commit is tagged and built by CI. |
| `develop` | Integration branch. Features merge here first. |
| `feature/<name>` | One feature or fix per branch, branched from `develop`. |

## Workflow
1. `git checkout develop && git pull`
2. `git checkout -b feature/my-change`
3. Commit with a message referencing the issue: `fix ramp clamp (#12)`
4. Push and open a pull request into `develop`
5. CI must be green — build, unit tests and static analysis all pass
6. Merge into `develop`; when a release is ready, merge `develop` into `main`

## Releasing
```bash
git checkout main
git merge develop
git tag v1.0.0
git push origin main --tags
```
The tag triggers the release job: a clean build with `FW_VERSION=1.0.0`,
binaries attached to a GitHub release, and notes generated from the commits.

## Versioning
Semantic versioning: `MAJOR.MINOR.PATCH`.
* MAJOR — incompatible change (e.g. new hardware revision)
* MINOR — new feature, backwards compatible
* PATCH — bug fix only

## Definition of done
* Builds with no warnings (`-Werror` is enabled)
* Unit tests pass
* cppcheck reports no issues
* Flash and RAM usage reviewed in the size report
