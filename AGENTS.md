# Autonomous Development Instructions

## Project

cshell is a small Linux shell written in C23. The repository builds a single `cs` executable from `cs.c`, with executable language examples under `examples/` and integration tests under `tests/`.

## Required workflow

1. Inspect existing Issues, Pull Requests, recent commits, and CI results before choosing work.
2. Prefer an existing Issue when it describes the next useful task. Create an Issue when an important missing task is discovered.
3. Work on a dedicated branch. Never commit directly to `main`.
4. Make one coherent change at a time.
5. Preserve existing behavior unless the task explicitly changes it.
6. Before opening or updating a PR, build and test using the repository's existing Makefile/CI commands.
7. When CI fails, inspect the failure and repair the root cause before unrelated work.
8. Keep commits focused and use clear commit messages.
9. Open or update a PR when the change is ready for review. Link the relevant Issue.
10. Do not merge PRs autonomously.

## Build and test

Preferred validation:

```sh
make
make test
```

The authoritative CI workflow is `.github/workflows/build.yml`. It compiles with C23, `-O3 -Wall -Wextra -Werror`, executes language examples, and runs the interactive integration test.

## Constraints

- Production code is C23.
- Keep the implementation simple and close to the existing architecture.
- Reuse existing functions and facilities before adding helpers.
- Avoid unnecessary abstractions, dependencies, or rewrites.
- Preserve existing naming and formatting conventions.
- Do not introduce C++, Python, Java, JavaScript, Rust, or a new build system.

## Autonomous behavior

At each run:

1. Determine repository state.
2. Inspect failing CI first.
3. Inspect open PRs and their review/CI state.
4. Inspect open Issues and select a concrete useful task.
5. Implement the smallest coherent improvement that can be completed safely.
6. Compile and run relevant tests.
7. Commit and push the branch.
8. Create/update a PR and document validation.
9. If no safe useful implementation is available, do not invent work; report why.

Never bypass failing tests merely to make CI green. Never rewrite unrelated code just to create activity.
