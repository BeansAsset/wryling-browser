# Contributing to Wryling

Thanks for your interest. Wryling is a personal project in beta, so things move
fast and plans change. Here's how to help without wasted effort.

## Reporting bugs and ideas

[Open an issue](https://github.com/BeansAsset/wryling-browser/issues/new) and include:

- what you did, what you expected, and what happened instead;
- your Mac model and macOS version;
- a screenshot or screen recording if it's visual.

Security problems go through the private process in [SECURITY.md](./SECURITY.md),
not public issues.

## Pull requests

For anything bigger than a small fix, open an issue first to agree on the
approach. That avoids building something that doesn't fit where Wryling is going.

When you do open a pull request:

- Build and run it on macOS (see [Building from source](./README.md#building-from-source)),
  and check light and dark appearance, hover and focus states, and that nothing
  else in the window moved.
- Follow Chromium's and Brave's code style. Name new Wryling code `wryling_*`,
  next to the feature it belongs to.
- Keep changes to Chromium's own files to a minimum. Prefer an override in
  `chromium_src/`, and keep any direct Chromium edit in a patch under
  `patches/`, as brave-core does.
- Write user-facing text in plain, short sentences, without em dashes.
- Describe what changed and how you tested it, with before and after
  screenshots for anything visual.

By contributing, you agree that your work is released under the
[Mozilla Public License 2.0](./LICENSE), like the rest of the project.
