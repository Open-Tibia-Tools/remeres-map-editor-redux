#!/usr/bin/env python3
"""
AI-powered Issue and PR triage system using EmbeddingGemma 2.
Runs zero-shot semantic embedding classification on GitHub Actions and local environments.
"""

import json
import os
import sys
from pathlib import Path
from typing import Dict, List, Tuple

import numpy as np


# Default labels with descriptions used when GitHub API is unavailable
DEFAULT_LABELS: Dict[str, str] = {
    "Type: Bug": "Inconsistencies, crashes, or issues which will cause a problem for users.",
    "Type: Enhancement": "New feature or improvement request.",
    "Type: Documentation": "Improvements or additions to documentation (README, wiki, code comments).",
    "Type: Info": "Further information requested or required.",
    "Type: Missing Content": "Content not yet implemented or not following accepted standards.",
    "Area: Brushes": "Changes related to the brush system (ground, wall, doodad, border, etc.).",
    "Area: Build": "Changes to CMake, vcpkg, CI/CD, compiler, or build configuration.",
    "Area: Data": "Changes to data files (data/ — materials, clients.xml, menubar.xml).",
    "Area: I/O": "Changes to map file I/O (OTBM, minimap export, etc.).",
    "Area: Live Editing": "Changes to the collaborative live editing system.",
    "Area: Rendering": "Changes to the OpenGL rendering pipeline.",
    "Area: Scripts": "Changes to Lua scripts, Lua API, automation scripts.",
    "Area: Source": "Changes to C++ source code in source/.",
    "Area: UI": "Changes to the wxWidgets GUI (palettes, dialogs, menus, toolbar).",
    "Good First Issue": "Good for newcomers — issues that are simple and well-scoped for first-time contributors.",
    "Priority: Critical": "Represents a risk to stable releases.",
    "Priority: High": "Represent a high impact in key areas of the editor.",
    "Priority: Medium": "This issue may be impactful and needs some attention.",
    "Priority: Low": "Minor impact.",
    "Status: Pending Review": "This PR or Issue requires a review.",
    "Status: Pending Test": "This PR or Issue requires more testing.",
}

# Domain keyword hints to enrich semantic embedding anchors
LABEL_HINTS: Dict[str, str] = {
    "Type: Bug": "bug report, crash, defect, error, regression, broken, failure, freeze, exception, assertion, fault",
    "Type: Enhancement": "feature request, new feature, suggestion, proposal, improvement, enhancement, add option, support for",
    "Type: Documentation": "documentation, docs, README, guide, comments, wiki, typos, manual, AGENTS.md, instructions, markdown",
    "Type: Info": "question, information requested, clarification, help, inquiry, discussion, support question",
    "Type: Missing Content": "missing items, missing sprites, missing assets, content not yet implemented, missing definitions, missing borders",
    "Area: Brushes": "brush, ground brush, wall, doodad, border, autoborder, terrain, carpet, table, door, eraser, placement",
    "Area: Build": "CMake, vcpkg, Ninja, compiler, MSVC, Clang, GCC, CI/CD, GitHub Actions, workflows, dependencies, build_ninja, compilation",
    "Area: Data": "materials.xml, clients.xml, menubar.xml, items.otb, appearances, data directory, assets, client version",
    "Area: I/O": "OTBM, map loading, map saving, file format, minimap export, persistence, compression, serialization, gzip",
    "Area: Live Editing": "live collaboration, multiplayer editing, socket, server, client, network sync, peer, packet",
    "Area: Rendering": "OpenGL, GLSL shaders, MapDrawer, chunk cache, NanoVG, textures, lighting, drawing, FPS, graphics, render, tile renderer",
    "Area: Scripts": "Lua API, lua scripts, script engine, extensions, test scripts, bindings, lua_api",
    "Area: Source": "C++ source code architecture, core refactoring, backend, engine, memory management, algorithms",
    "Area: UI": "wxWidgets GUI, dialogs, palettes, menus, buttons, toolbars, windows, controls, theme, layout, canvas, view, icons, preferences",
}

# Labels that should never be automatically applied by triage
NON_TRIAGE_LABELS = {"Good First Issue", "Stale"}


def fetch_repository_labels(repo) -> Dict[str, str]:
    """
    Fetches all existing labels and their descriptions from GitHub.
    Returns a dict of {exact_name: description}.
    """
    labels_dict = {}
    try:
        for label in repo.get_labels():
            labels_dict[label.name] = label.description or ""
    except Exception as err:
        print(f"Warning: Error fetching repository labels: {err}")
    return labels_dict


def build_label_anchor(name: str, desc: str) -> str:
    """
    Builds the text for a label embedding anchor with task instruction prefix.
    """
    parts = [name]
    if desc:
        parts.append(desc.strip())
    if name in LABEL_HINTS:
        parts.append(f"Keywords: {LABEL_HINTS[name]}")
    content = ". ".join(parts)
    return f"task: classification | text: {content}"


def build_target_text(title: str, body: str, is_pr: bool, changed_files: List[str]) -> str:
    """
    Builds the target text for the issue or pull request with task instruction prefix.
    """
    lines = [
        f"Target: {'Pull Request' if is_pr else 'Issue'}",
        f"Title: {title.strip()}",
        f"Description:\n{body[:3000].strip()}",
    ]
    if is_pr and changed_files:
        lines.append("Changed Files (up to 30):\n" + "\n".join(f"- {f}" for f in changed_files[:30]))

    content = "\n".join(lines)
    return f"task: classification | text: {content}"


class TransformersBackend:
    """Native Transformers backend using PyTorch CPU for EmbeddingGemma 2."""
    def __init__(self, model_name: str = "google/embeddinggemma-2"):
        import torch
        from transformers import AutoModel, AutoTokenizer

        print(f"Loading {model_name} (text-only backbone) via Transformers...")
        self.tokenizer = AutoTokenizer.from_pretrained(model_name)
        self.model = AutoModel.from_pretrained(model_name, vision_config=None, audio_config=None)
        self.model.eval()
        self.torch = torch

    def embed(self, texts: List[str]) -> np.ndarray:
        import torch.nn.functional as F

        inputs = self.tokenizer(texts, return_tensors="pt", padding=True, truncation=True, max_length=2048)
        with self.torch.no_grad():
            outputs = self.model(**inputs)
            mask = inputs["attention_mask"].unsqueeze(-1).expand(outputs.last_hidden_state.size()).float()
            sum_embeddings = self.torch.sum(outputs.last_hidden_state * mask, 1)
            sum_mask = self.torch.clamp(mask.sum(1), min=1e-9)
            mean_pooled = sum_embeddings / sum_mask
            normalized = F.normalize(mean_pooled, p=2, dim=1)
            return normalized.cpu().numpy()


def get_embedding_model() -> TransformersBackend:
    """
    Returns the initialized Transformers embedding backend.
    """
    return TransformersBackend("google/embeddinggemma-2")


def compute_embeddings(backend: TransformersBackend, texts: List[str]) -> np.ndarray:
    return backend.embed(texts)


def select_labels_from_scores(
    scores: Dict[str, float],
    type_threshold: float = 0.70,
    area_threshold: float = 0.75,
    area_delta: float = 0.03,
) -> Tuple[List[str], str]:
    """
    Selects 1-3 labels from similarity scores using category partitioning:
    - 1 Type: label (highest score among Type:*, if score >= type_threshold)
    - 1 to 2 Area: labels (highest score among Area:*, if score >= area_threshold)
    Returns (selected_labels, summary_log).
    """
    log_lines = ["--- Label Similarity Rankings ---"]
    for lbl, score in sorted(scores.items(), key=lambda x: x[1], reverse=True):
        log_lines.append(f"  [{score:.3f}] {lbl}")

    selected: List[str] = []

    # 1. Select primary Type: label
    type_candidates = sorted(
        [(k, v) for k, v in scores.items() if k.startswith("Type:")],
        key=lambda x: x[1],
        reverse=True,
    )
    if type_candidates:
        best_type, type_score = type_candidates[0]
        if type_score >= type_threshold:
            selected.append(best_type)
            log_lines.append(f"\nSelected Type: '{best_type}' (score: {type_score:.3f} >= {type_threshold})")
        else:
            log_lines.append(f"\nSkipped Type: '{best_type}' (score: {type_score:.3f} < threshold {type_threshold})")

    # 2. Select relevant Area: labels
    area_candidates = sorted(
        [(k, v) for k, v in scores.items() if k.startswith("Area:")],
        key=lambda x: x[1],
        reverse=True,
    )
    if area_candidates:
        best_area, area_score = area_candidates[0]
        if area_score >= area_threshold:
            selected.append(best_area)
            log_lines.append(f"Selected primary Area: '{best_area}' (score: {area_score:.3f} >= {area_threshold})")

            # Check secondary Area label
            if len(area_candidates) > 1:
                second_area, second_score = area_candidates[1]
                if second_score >= area_threshold and (area_score - second_score) <= area_delta:
                    selected.append(second_area)
                    log_lines.append(
                        f"Selected secondary Area: '{second_area}' (score: {second_score:.3f}, delta: {area_score - second_score:.3f} <= {area_delta})"
                    )

    summary_log = "\n".join(log_lines)
    return selected, summary_log


def run_inference(
    title: str,
    body: str,
    is_pr: bool,
    changed_files: List[str],
    available_labels: Dict[str, str],
) -> Tuple[List[str], str]:
    """
    Runs zero-shot semantic embedding classification using EmbeddingGemma 2.
    Returns (suggested_labels, summary_log).
    """
    backend = get_embedding_model()

    # 1. Compute target text embedding
    target_prompt = build_target_text(title=title, body=body, is_pr=is_pr, changed_files=changed_files)
    print("Computing target embedding...")
    target_vec = compute_embeddings(backend, [target_prompt])[0]

    # 2. Compute similarity against candidate labels (excluding manual/status labels)
    candidate_names = [
        name for name in available_labels.keys()
        if not (name.startswith("Status:") or name.startswith("Priority:") or name in NON_TRIAGE_LABELS)
    ]
    label_anchors = [build_label_anchor(name, available_labels.get(name, "")) for name in candidate_names]

    print(f"Scoring {len(candidate_names)} candidate labels...")
    label_vecs = compute_embeddings(backend, label_anchors)

    scores: Dict[str, float] = {}
    for idx, name in enumerate(candidate_names):
        scores[name] = float(target_vec @ label_vecs[idx])

    # 3. Select labels based on category partitioning and similarity thresholds
    suggested, summary_log = select_labels_from_scores(scores)
    return suggested, summary_log


def main():
    event_name = os.environ.get("EVENT_NAME", "").strip()
    token = os.environ.get("GITHUB_TOKEN", "").strip()
    repo_name = os.environ.get("GITHUB_REPOSITORY", "").strip()
    raw_context = os.environ.get("EVENT_CONTEXT", "{}")
    dry_run = os.environ.get("DRY_RUN", "").lower() in ("true", "1", "yes")

    print(f"=== Starting AI Triage System (EmbeddingGemma-2) ===")
    print(f"Event Name: {event_name}")
    print(f"Repository: {repo_name}")
    print(f"Dry Run Mode: {dry_run}")

    # 1. Check if this is a push / cache-seed event
    if event_name == "push":
        print("Push trigger detected on default branch. Pre-warming model cache...")
        _ = get_embedding_model()
        print("Model successfully cached. Exiting triage.")
        return 0

    # 2. Parse event context
    try:
        event_data = json.loads(raw_context) if isinstance(raw_context, str) else raw_context
    except Exception as e:
        print(f"Error parsing EVENT_CONTEXT JSON: {e}")
        event_data = {}

    is_pr = False
    number = None
    title = ""
    body = ""

    if event_name == "issues":
        issue_info = event_data.get("issue", {})
        number = issue_info.get("number")
        title = issue_info.get("title", "")
        body = issue_info.get("body", "") or ""
        is_pr = False
    elif event_name in ("pull_request_target", "pull_request"):
        pr_info = event_data.get("pull_request", {})
        number = pr_info.get("number")
        title = pr_info.get("title", "")
        body = pr_info.get("body", "") or ""
        is_pr = True
    elif event_name == "workflow_dispatch":
        inputs = event_data.get("inputs", {})
        input_num = inputs.get("number")
        if input_num:
            try:
                number = int(input_num)
            except ValueError:
                print(f"Invalid input number: {input_num}")
    else:
        # Fallback inspection for manual or simulation runs
        if "issue" in event_data:
            number = event_data["issue"].get("number")
            title = event_data["issue"].get("title", "")
            body = event_data["issue"].get("body", "") or ""
            is_pr = False
        elif "pull_request" in event_data:
            number = event_data["pull_request"].get("number")
            title = event_data["pull_request"].get("title", "")
            body = event_data["pull_request"].get("body", "") or ""
            is_pr = True

    # 3. Initialize GitHub client and retrieve repo information
    gh = None
    gh_repo = None
    repo_labels: Dict[str, str] = {}
    changed_files: List[str] = []

    if token and repo_name:
        from github import Auth, Github
        try:
            gh = Github(auth=Auth.Token(token))
            gh_repo = gh.get_repo(repo_name)
            repo_labels = fetch_repository_labels(gh_repo)
            print(f"Retrieved {len(repo_labels)} valid labels from repository.")
        except Exception as e:
            print(f"Warning: Failed to connect to GitHub API: {e}")

    # If title is missing (e.g. workflow_dispatch), fetch details from GitHub API
    if gh_repo and number and not title:
        try:
            target_item = gh_repo.get_issue(number)
            title = target_item.title
            body = target_item.body or ""
            is_pr = target_item.pull_request is not None
            print(f"Fetched details for {'PR' if is_pr else 'Issue'} #{number} from API.")
        except Exception as err:
            print(f"Error fetching issue/PR #{number}: {err}")

    if not number or not title:
        print("No active Issue or PR detected in event context. Exiting.")
        return 0

    print(f"Processing {'Pull Request' if is_pr else 'Issue'} #{number}: {title}")

    # Fallback to local default labels if API unavailable (e.g. local offline test)
    if not repo_labels:
        repo_labels = dict(DEFAULT_LABELS)
        print(f"Using default fallback label set ({len(repo_labels)} labels).")

    # For PRs: fetch changed files (up to 30)
    if is_pr and gh_repo and number:
        try:
            pr_obj = gh_repo.get_pull(number)
            for file_item in pr_obj.get_files():
                changed_files.append(file_item.filename)
                if len(changed_files) >= 30:
                    break
            print(f"Retrieved {len(changed_files)} changed files for PR #{number}.")
        except Exception as err:
            print(f"Warning: Could not fetch changed files for PR #{number}: {err}")
    elif is_pr and "changed_files" in event_data:
        changed_files = event_data["changed_files"][:30]

    # 4. Run semantic embedding inference
    raw_suggested, summary_log = run_inference(
        title=title,
        body=body,
        is_pr=is_pr,
        changed_files=changed_files,
        available_labels=repo_labels,
    )

    print("\n" + summary_log + "\n")

    # 5. Filter and strictly validate labels against repository labels
    label_lookup = {k.lower().strip(): k for k in repo_labels.keys()}
    valid_labels_to_apply = []

    for item in raw_suggested:
        clean_item = str(item).strip()
        if clean_item in repo_labels:
            if clean_item not in valid_labels_to_apply:
                valid_labels_to_apply.append(clean_item)
        elif clean_item.lower() in label_lookup:
            exact_match = label_lookup[clean_item.lower()]
            if exact_match not in valid_labels_to_apply:
                valid_labels_to_apply.append(exact_match)
        else:
            print(f"Discarding invalid label: '{clean_item}'")

    print(f"Suggested raw labels: {raw_suggested}")
    print(f"Validated labels to apply: {valid_labels_to_apply}")

    # 6. Apply labels to Issue or PR
    if not valid_labels_to_apply:
        print("No valid matching labels found to apply.")
        return 0

    if dry_run or not gh_repo:
        print(f"[DRY-RUN] Would apply labels {valid_labels_to_apply} to #{number}.")
        return 0

    try:
        issue_obj = gh_repo.get_issue(number)
        issue_obj.add_to_labels(*valid_labels_to_apply)
        print(f"Successfully applied {valid_labels_to_apply} to #{number} via GitHub API.")
    except Exception as e:
        print(f"Error applying labels to #{number}: {e}")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
