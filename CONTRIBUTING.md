# Contributing

Thank you for your interest in contributing. This project is released under the MIT License and uses the **Developer Certificate of Origin (DCO)** to manage contributions.

By submitting a contribution (code, documentation, hardware design files, etc.), you agree that it may be distributed under the MIT License of this repository.

---

## Ways to contribute

- **Bug reports**
  - Use the GitHub "Issues" tab.
  - Describe the problem clearly and minimally:
    - Steps to reproduce
    - Expected vs. actual behavior
    - Environment (OS, compiler/tool versions, FPGA board revision, etc.)

- **Feature requests / design proposals**
  - Open an issue before starting major work.
  - Briefly describe:
    - The problem you want to solve
    - The proposed solution or interface
    - Any alternatives you considered

- **Pull requests (PRs)**
  - For small fixes (typos, small bug fixes), you can submit a PR directly.
  - For larger changes, link the PR to a corresponding issue.
  - Keep PRs focused: one logical change per PR where possible.

---

## Development guidelines

These are intentionally minimal; adapt as needed for your contributions.

- Follow the existing **code style** and structure where reasonably possible.
- Keep changes **modular and documented**:
  - Update comments and documentation when changing behavior or interfaces.
  - Add or update tests/examples if applicable.
- Avoid mixing formatting-only changes with functional changes in the same commit/PR.

Practical guidance:

- If you are new to the project, start with `HACKING.md`.
- If you do not have hardware, docs, Python, C++, recipes, and HDL simulation are all useful contribution areas.
- If you do have hardware, measured workflows, validated examples, and board-setup notes are especially valuable.
- Docs, examples, and test infrastructure are first-class contributions, not second-tier ones.
- Worked examples are especially encouraged.
- Wiring diagrams, timing diagrams, screenshots, scope traces, logic-analyzer captures, and setup photos are valuable contributions too.

If in doubt, prefer clarity and maintainability over cleverness.

---

## New contributor paths

Some common contribution paths are:

- **Docs and onboarding**
  - Improve explanations, examples, and contributor guidance
- **C++ tools**
  - Improve command-line UX, parsing, and hardware wrappers
- **Python bindings**
  - Improve examples, packaging, and API coverage
- **RTL and simulation**
  - Improve IP blocks, CDC/reset structure, and test benches
- **Hardware validation**
  - Verify workflows on a real board and turn them into durable docs
- **Real-world example contributions**
  - Share practical experiment workflows, instrument integrations, and validated setup notes

See also:

- `HACKING.md`
- `docs/docs/hacking.md`
- `docs/docs/getting_started_no_hardware.md`
- `docs/docs/getting_started_hardware.md`

---

## Developer Certificate of Origin (DCO)

This project uses the **Developer Certificate of Origin (DCO) 1.1**.
The DCO is a simple statement that you, as a contributor, have the right to submit your work and that you license it under the same terms as this project.

The full text of the DCO is available at:

> https://developercertificate.org/

By contributing, you certify that your contribution complies with the DCO.

---

## Sign-off requirement

**Every commit included in a pull request must be "signed off"** to indicate agreement with the DCO.

A sign-off is a line at the end of the commit message of the form:

```text
Signed-off-by: Full Name <email@example.com>
```

- Use your **real name** (no pseudonyms or initials only).
- The email should be an address where you can be reached and that you control.
- The sign-off must match the identity of the author of the commit.

### How to add a sign-off

The easiest way is to let Git add it automatically:

```bash
git commit -s
```

This appends the `Signed-off-by` line using your configured Git identity:

```bash
git config user.name "Full Name"
git config user.email "email@example.com"
```

You can also add the line manually at the end of the commit message.

### Amending commits without sign-off

If you forgot to sign off a commit, you can amend:

```bash
git commit --amend -s
```

and then force-push your branch:

```bash
git push --force-with-lease
```

For multiple commits, you can use an interactive rebase:

```bash
git rebase -i origin/main
# mark commits for edit and add -s to each using `git commit --amend -s`
git push --force-with-lease
```

Pull requests with commits lacking a proper DCO sign-off may be asked to amend their commit history before they can be merged.

---

## Licensing of contributions

Unless explicitly stated otherwise, by submitting a contribution you agree that:

- Your contribution is licensed under the **MIT License** of this repository.
- You grant the project maintainers the right to use, modify, and redistribute your contribution as part of the project under that license.

If you include third-party code or designs, make sure that:

- They are compatible with the MIT License, and
- You clearly indicate the original source and license where required.

---

## Questions

If anything in this CONTRIBUTING guide or the DCO is unclear for your use case (e.g. employer IP policies, third-party
reuse, hardware design licensing), feel free to open an issue for clarification before submitting a PR.
