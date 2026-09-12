#!/usr/bin/env python3
"""
AI-powered Issue and PR triage system using local Qwen3.5-0.8B GGUF via llama-cpp-python.
Runs on GitHub Actions CPU runners with optimized thinking-mode settings and PR file-change context.
"""

import os
import sys
import json
import re
from pathlib import Path
from typing import List, Dict, Tuple, Optional


def get_cache_dir() -> Path:
    cache_env = os.environ.get("CACHE_DIR")
    if cache_env:
        path = Path(cache_env)
    else:
        path = Path.home() / ".cache" / "models"
    path.mkdir(parents=True, exist_ok=True)
    return path


def download_model() -> str:
    """
    Downloads or verifies unsloth/Qwen3.5-0.8B-GGUF (Qwen3.5-0.8B-Q4_K_M.gguf)
    in ~/.cache/models using huggingface_hub.
    """
    from huggingface_hub import hf_hub_download

    cache_dir = get_cache_dir()
    model_filename = "Qwen3.5-0.8B-Q4_K_M.gguf"
    target_file = cache_dir / model_filename

    if target_file.is_file() and target_file.stat().st_size > 100_000_000:
        print(f"Model already cached: {target_file} ({target_file.stat().st_size / (1024 * 1024):.1f} MB)")
        return str(target_file)

    print(f"Downloading {model_filename} from unsloth/Qwen3.5-0.8B-GGUF to {cache_dir}...")
    model_path = hf_hub_download(
        repo_id="unsloth/Qwen3.5-0.8B-GGUF",
        filename=model_filename,
        local_dir=str(cache_dir),
    )
    print(f"Model downloaded successfully to {model_path}")
    return model_path


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


def extract_labels_from_output(text: str) -> List[str]:
    """
    Extracts label list from model output via regex patterns,
    handling JSON code fences, raw JSON objects, and inline lists.
    """
    # 1. Search for ```json { "labels": [...] } ``` blocks
    code_blocks = re.findall(r"```(?:json)?\s*(\{.*?\})\s*```", text, re.DOTALL)
    for block in code_blocks:
        try:
            data = json.loads(block)
            if isinstance(data, dict) and "labels" in data and isinstance(data["labels"], list):
                return [str(item) for item in data["labels"]]
        except Exception:
            pass

    # 2. Search for raw JSON objects with a "labels" key
    json_objects = re.findall(r"\{[^{}]*\"labels\"\s*:\s*\[[^\]]*\][^{}]*\}", text, re.DOTALL)
    for obj_str in json_objects:
        try:
            data = json.loads(obj_str)
            if isinstance(data, dict) and "labels" in data and isinstance(data["labels"], list):
                return [str(item) for item in data["labels"]]
        except Exception:
            pass

    # 3. Fallback: match "labels": [ ... ] directly
    arr_match = re.search(r"\"labels\"\s*:\s*(\[[^\]]*\])", text, re.DOTALL)
    if arr_match:
        try:
            labels_arr = json.loads(arr_match.group(1))
            if isinstance(labels_arr, list):
                return [str(item) for item in labels_arr]
        except Exception:
            pass

    return []


def run_inference(
    model_path: str,
    title: str,
    body: str,
    is_pr: bool,
    number: int,
    changed_files: List[str],
    available_labels: Dict[str, str],
) -> Tuple[List[str], str]:
    """
    Runs inference using llama-cpp-python with Qwen3.5 thinking mode.
    Returns (suggested_labels, raw_model_output).
    """
    from llama_cpp import Llama

    cpu_threads = os.cpu_count() or 4
    print(f"Initializing Llama model from {model_path} with {cpu_threads} CPU threads...")
    llm = Llama(
        model_path=model_path,
        n_ctx=4096,
        n_threads=cpu_threads,
        verbose=False,
    )

    # Format available labels with descriptions
    label_lines = []
    for name, desc in available_labels.items():
        if desc:
            label_lines.append(f"- {name}: {desc}")
        else:
            label_lines.append(f"- {name}")
    labels_context = "\n".join(label_lines)

    system_prompt = (
        "You are an automated Issue and Pull Request triage system for Remere's Map Editor (RME).\n"
        "Your task is to analyze the title, description, and changed files (for Pull Requests), "
        "and select the most accurate labels strictly from the Allowed Labels list below.\n\n"
        "Allowed Labels:\n"
        f"{labels_context}\n\n"
        "Instructions:\n"
        "1. First, think step-by-step inside <think>...</think> tags. Analyze the context, what component is touched, "
        "and determine the matching Type, Area, and/or Priority labels.\n"
        "2. Only select labels that EXACTLY match one of the Allowed Labels.\n"
        "3. Select 1 to 4 appropriate labels (e.g. one Type: label, relevant Area: labels).\n"
        "4. After </think>, output ONLY a valid JSON object in this format:\n"
        '{"labels": ["label1", "label2"]}\n'
        "5. Do not include any additional commentary, markdown notes, or text outside the JSON object."
    )

    user_prompt = f"Target: {'Pull Request' if is_pr else 'Issue'} #{number}\nTitle: {title}\nDescription:\n{body}\n"
    if is_pr and changed_files:
        user_prompt += "\nChanged Files (up to 30):\n" + "\n".join(f"- {f}" for f in changed_files)

    messages = [
        {"role": "system", "content": system_prompt},
        {"role": "user", "content": user_prompt},
    ]

    print("Running LLM inference with temperature=0.5, max_tokens=1024...")
    response = llm.create_chat_completion(
        messages=messages,
        temperature=0.5,
        max_tokens=1024,
    )

    content = response["choices"][0]["message"]["content"]
    suggested = extract_labels_from_output(content)
    return suggested, content


def main():
    event_name = os.environ.get("EVENT_NAME", "").strip()
    token = os.environ.get("GITHUB_TOKEN", "").strip()
    repo_name = os.environ.get("GITHUB_REPOSITORY", "").strip()
    raw_context = os.environ.get("EVENT_CONTEXT", "{}")
    dry_run = os.environ.get("DRY_RUN", "").lower() in ("true", "1", "yes")

    print(f"=== Starting AI Triage System ===")
    print(f"Event Name: {event_name}")
    print(f"Repository: {repo_name}")
    print(f"Dry Run Mode: {dry_run}")

    # 1. Download or verify cached model
    model_path = download_model()

    # 2. Check if this is a push / cache-seed event
    if event_name == "push":
        print("Push trigger detected on default branch. Model successfully cached. Exiting triage.")
        return 0

    # 3. Parse event context
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

    # 4. Initialize GitHub client and retrieve repo information
    gh = None
    gh_repo = None
    repo_labels: Dict[str, str] = {}
    changed_files: List[str] = []

    if token and repo_name:
        from github import Github, Auth
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
        default_labels = [
            "Type: Bug", "Type: Enhancement", "Type: Documentation", "Type: Info", "Type: Missing Content",
            "Area: Brushes", "Area: Build", "Area: Data", "Area: I/O", "Area: Live Editing",
            "Area: Rendering", "Area: Scripts", "Area: Source", "Area: UI",
            "Priority: Critical", "Priority: High", "Priority: Medium", "Priority: Low",
            "Status: Pending Review", "Status: Pending Test", "Good First Issue"
        ]
        repo_labels = {lbl: "" for lbl in default_labels}
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

    # 5. Run LLM inference
    raw_suggested, raw_output = run_inference(
        model_path=model_path,
        title=title,
        body=body,
        is_pr=is_pr,
        number=number,
        changed_files=changed_files,
        available_labels=repo_labels,
    )

    print("\n--- Model Output ---")
    print(raw_output)
    print("--------------------\n")

    # 6. Filter and strictly validate labels against repository labels
    # Build case-insensitive lookup
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
            print(f"Discarding invalid/hallucinated label: '{clean_item}'")

    print(f"Suggested raw labels: {raw_suggested}")
    print(f"Validated labels to apply: {valid_labels_to_apply}")

    # 7. Apply labels to Issue or PR
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
