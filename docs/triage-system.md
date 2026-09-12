# AI-Powered Issue and PR Triage

This document describes the automated triage system utilizing a local small LLM (Qwen3.5-0.8B GGUF) via llama-cpp-python on GitHub Actions.

## Capabilities
- Analyzes newly opened issues and pull requests.
- Reviews title, description, and list of changed files.
- Automatically selects relevant labels from existing repository labels (such as Type: Documentation, Type: Bug, Area: UI, etc.).
