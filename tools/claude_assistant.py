"""Claude Code Game Studios Assistant
===================================

This tiny helper script demonstrates how the *agents* defined in
``workflow/claude/agents.json`` can be used to drive an automated workflow
with Claude (or any other LLM that supports a similar API).  The idea is to
replicate a **virtual studio** where each agent has a clear specialization and
the script picks the most appropriate one for a given task.

Usage::

    python tools/claude_assistant.py "Implement a real font renderer using stb_truetype"

The script will:

1. Load the agents definition.
2. Infer which agent's ``skills`` best match the requested task.
3. Build a prompt that sets the role, context (project structure) and the
   specific request.
4. Send the prompt to Claude via its HTTP API (if the ``CLAUDE_API_KEY``
   environment variable is set).  If the key is missing, the script prints the
   prompt that *would* be sent, so developers can copy‑paste it manually.

The script is deliberately lightweight – it does not attempt to automatically
apply patches, but it prints the generated response to ``stdout`` so the user
can review it and apply it manually (or pipe it into ``apply_patch`` in the
future).
"""

import json
import os
import sys
import textwrap
from collections import Counter

# ---------------------------------------------------------------------------
# Load agents definition
# ---------------------------------------------------------------------------
AGENTS_PATH = os.path.join(os.path.dirname(__file__), "..", "workflow", "claude", "agents.json")

def load_agents() -> list:
    """Load the agents JSON file and return a list of dicts."""
    try:
        with open(AGENTS_PATH, "r", encoding="utf-8") as f:
            data = json.load(f)
        return data.get("agents", [])
    except FileNotFoundError:
        print(f"[error] agents definition not found at {AGENTS_PATH}", file=sys.stderr)
        sys.exit(1)

# ---------------------------------------------------------------------------
# Simple skill‑matching heuristic
# ---------------------------------------------------------------------------
def match_agent(task: str, agents: list) -> dict:
    """Return the agent whose skill list matches the task best.

    The heuristic tokenises the task, lower‑cases words and counts intersections
    with each agent's ``skills`` list.  The agent with the highest count wins.
    If there is a tie, the first encountered agent is selected.
    """
    task_words = [w.strip(".,!?:;()[]\n\r").lower() for w in task.split()]
    best = None
    best_score = -1
    for agent in agents:
        skill_set = {s.lower() for s in agent.get("skills", [])}
        score = sum(1 for w in task_words if w in skill_set)
        if score > best_score:
            best_score = score
            best = agent
    return best

# ---------------------------------------------------------------------------
# Prompt construction
# ---------------------------------------------------------------------------
def build_prompt(agent: dict, task: str) -> str:
    """Create a prompt for Claude based on the selected agent.

    The prompt contains:
    * The agent's role (first‑person description).
    * A brief overview of the project layout (relevant files).
    * The concrete task the user asked for.
    """
    role = agent.get("role", "")
    # Brief project description – we embed the most relevant files for the UI
    # task.  This can be expanded later if needed.
    project_overview = textwrap.dedent(
        """
        Project: AoM Native Port
        Relevant source files:
        - src/engine/engine.cpp / include/engine/engine.h
        - src/renderer/gles_renderer.cpp / include/renderer/renderer.h
        - src/ui/ui.cpp / include/ui/ui.h
        - third_party/stb_truetype.h / third_party/stb_truetype.c
        """
    ).strip()

    prompt = (
        f"You are a {agent['name']} in a virtual game‑development studio. "
        f"Your role: {role}\n\n"
        f"Project overview:\n{project_overview}\n\n"
        f"Task: {task}\n\n"
        "Please provide the implementation as a concise code snippet or a series "
        "of patches (using the standard *** Begin Patch / *** End Patch format) "
        "that can be directly applied to the repository."
    )
    return prompt

# ---------------------------------------------------------------------------
# Claude API helper (optional – works if CLAUDE_API_KEY env var is set)
# ---------------------------------------------------------------------------
def call_claude_api(prompt: str) -> str:
    """Send *prompt* to Claude and return the textual response.

    This function uses the Anthropic Claude API v1.  It is kept minimal – only the
    ``messages`` payload is sent.  If the environment variable ``CLAUDE_API_KEY``
    is missing, the function raises a ``RuntimeError``.
    """
    api_key = os.getenv("CLAUDE_API_KEY")
    if not api_key:
        raise RuntimeError("CLAUDE_API_KEY environment variable not set")

    import requests

    url = "https://api.anthropic.com/v1/messages"
    headers = {
        "x-api-key": api_key,
        "anthropic-version": "2023-06-01",
        "content-type": "application/json",
    }
    payload = {
        "model": "claude-3-sonnet-20240229",  # adjust model as needed
        "max_tokens": 4096,
        "temperature": 0.0,
        "messages": [{"role": "user", "content": prompt}],
    }
    resp = requests.post(url, headers=headers, json=payload, timeout=120)
    resp.raise_for_status()
    data = resp.json()
    for block in data.get("content", []):
        if block.get("type") == "text":
            return block.get("text", "")
    return ""

# ---------------------------------------------------------------------------
# Main entry point
# ---------------------------------------------------------------------------
def main(argv=None):
    if argv is None:
        argv = sys.argv[1:]
    if not argv:
        print("Usage: python tools/claude_assistant.py \"<task description>\"")
        sys.exit(0)

    task = " ".join(argv).strip()
    agents = load_agents()
    agent = match_agent(task, agents)
    if not agent:
        print("[error] No suitable agent found for the task.", file=sys.stderr)
        sys.exit(1)

    prompt = build_prompt(agent, task)
    print("--- Prompt sent to Claude ---\n")
    print(prompt)
    print("\n--- End of prompt ---\n")

    try:
        response = call_claude_api(prompt)
        print("--- Claude response ---\n")
        print(response)
        print("\n--- End of response ---")
    except Exception as e:
        print(f"[warning] Could not call Claude API: {e}")
        print("You can copy the prompt above and paste it manually into the Claude web UI.")

if __name__ == "__main__":
    main()
