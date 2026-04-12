# Workspace Rules

- Prefer the smallest correct change. Do not refactor unrelated code while implementing a focused task.
- Do not modify unrelated dirty worktree files. If a requested change conflicts with existing uncommitted edits, stop and ask.
- Never commit without the user's explicit request. When committing, stage only files relevant to the task.
- Run the narrowest relevant build/test target for touched code. If verification cannot be run, say so clearly.
- When changing a user-facing CLI, test grammar, or API, update the corresponding docs in the same task.
- Keep generic reusable code separate from device-specific helpers.
- Do not copy, move, or re-own hardware-facing runtime objects during refactors. Keep the hardware-owning instance anchored and pass access through pointers or references.
- Do not make whitespace-only or alignment-only edits unless the user explicitly asks for formatting changes.
- When editing an existing file, preserve its current indentation, alignment, and surrounding formatting unless a formatting change is required for correctness.
- Before finishing, check touched files for accidental spacing-only diffs and revert them.
