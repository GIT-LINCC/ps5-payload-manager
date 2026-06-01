# Stop Running Payloads Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a real stop action for payloads that can be matched to a running PS5 process.

**Architecture:** Keep file management separate from runtime process control. The native backend scans processes by payload filename, exposes running-state and stop routes, and launches future payloads through `file:/...` so modern elfldr names the spawned process after the file. The frontend shows a stop button only when the backend reports an installed payload is running.

**Tech Stack:** C daemon with libmicrohttpd, PS5 payload SDK process sysctl APIs, React/Vite frontend, Express mock backend.

---

### Task 1: Backend Runtime Process Control

**Files:**
- Modify: `include/pldmgr.h`
- Modify: `include/ps5_launcher.h`
- Modify: `src/ps5_launcher.c`
- Modify: `src/main.c`

- [x] Add route constants for `/running_payloads` and `/stop_payload`.
- [x] Add process-scan helpers that match safe payload filenames to `ki_comm`.
- [x] Add JSON output for installed payloads that are currently visible as processes.
- [x] Add stop handler that validates filename, resolves it through the payload manager, and sends `SIGKILL` only to matching payload process names.
- [x] Change `ps5_launch_elf()` to send `file:/absolute/path` to elfldr, preserving the existing raw-byte sender as a fallback helper if later needed.

### Task 2: Frontend Runtime Controls

**Files:**
- Modify: `frontend/src/App.jsx`
- Modify: `frontend/src/components/views/StorageHub.jsx`
- Modify: `frontend/src/i18n.js`

- [x] Fetch `/running_payloads` with payload refresh and when opening storage.
- [x] Add a stop confirmation modal and call `/stop_payload?filename=...`.
- [x] Pass running filenames into `StorageHub`.
- [x] Add a stop button next to installed payloads that are reported as running.
- [x] Add English and Simplified Chinese strings for stop actions and warnings.

### Task 3: Mock Backend

**Files:**
- Modify: `frontend/mock-server.js`

- [x] Track mock running payload filenames when `/loadpayload:*` is called.
- [x] Return mock state from `/running_payloads`.
- [x] Remove a filename from mock state through `/stop_payload`.

### Task 4: Verification

**Commands:**
- `cd frontend && npx eslint src/components/views/SettingsView.jsx src/i18n.js`
- `cd frontend && npx eslint src/App.jsx src/components/views/StorageHub.jsx src/components/views/SettingsView.jsx src/i18n.js`
- `cd frontend && npm run build`
- `wsl.exe -d Ubuntu-24.04 -- bash -lc 'cd /mnt/d/ps5/ps5-payload-manager; make -f <(tr -d "\r" < Makefile) clean all'`

- [x] Confirm frontend build succeeds.
- [x] Confirm focused Settings/i18n eslint succeeds; broader App/StorageHub lint still reports existing React compiler and unused-variable findings.
- [x] Confirm `pldmgr.elf` is rebuilt.
