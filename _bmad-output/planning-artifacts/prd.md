---
stepsCompleted:
  - step-01-init
  - step-02-discovery
  - step-03-success
  - step-04-journeys
  - step-05-domain
  - step-06-innovation
  - step-07-project-type
  - step-08-scoping
  - step-09-functional
  - step-10-nonfunctional
  - step-11-polish
inputDocuments:
  - docs/index.md
  - docs/project-overview.md
  - docs/architecture.md
  - docs/source-tree-analysis.md
  - docs/development-guide.md
documentCounts:
  briefs: 0
  research: 0
  brainstorming: 0
  projectDocs: 5
workflowType: 'prd'
projectType: 'brownfield'
classification:
  projectType: desktop_app
  domain: developer_tools
  complexity: medium
  projectContext: brownfield
featureSummary: "Multiple Breakpoints & Watchpoints with Dedicated UI for BeastEm Z80 Emulator"
---

# Product Requirements Document - beastem

**Author:** Ant
**Date:** 2026-01-29

## Executive Summary

**Feature:** Multiple Breakpoints & Watchpoints with Dedicated UI

Enhance BeastEm's debugging capabilities by adding support for multiple breakpoints and watchpoints, replacing the current single-breakpoint inline editing with a dedicated tabbed UI screen. This feature builds prerequisites for future DeZog DZRP 2.0 protocol integration, enabling VSCode-based debugging.

## Success Criteria

### User Success

- **Effortless debugging**: Developers can set up complex debugging scenarios (multiple breakpoints, memory watchpoints) without friction
- **Accuracy**: Breakpoints trigger reliably at the exact instruction; watchpoints catch every memory access in the monitored range
- **Visual consistency**: New UI feels native to BeastEm - matches existing aesthetic and interaction patterns
- **Clear status**: At a glance, developers know what's being monitored and what's enabled/disabled

### Technical Success

- **Follow existing patterns**: Code integrates naturally with BeastEm's existing architecture and style
- **Clean foundation**: Implementation supports future DeZog DZRP 2.0 integration without major rework
- **Hidden system breakpoints**: 2 reserved watchpoints available for programmatic use (step-over, step-out support)

### Measurable Outcomes

| Metric | Target |
|--------|--------|
| User breakpoints | 8 supported |
| User watchpoints | 8 supported |
| System breakpoints | 2 (hidden) |
| Breakpoint accuracy | 100% - never miss a trigger |
| Watchpoint accuracy | 100% - catch all reads/writes in range |
| UI pages | 2 tabs (Breakpoints / Watchpoints) |

## Product Scope

### MVP - Minimum Viable Product

- 8 user-configurable breakpoints (address, optional bank, enabled flag)
- 8 user-configurable watchpoints (address, range, type: read/write/both, enabled flag)
- 2 hidden system breakpoints for future DeZog support
- Dedicated UI screen with Breakpoints and Watchpoints tabs
- View, add, edit, enable/disable, delete breakpoints and watchpoints
- Integration with existing emulation loop for accurate triggering

### Vision (Future)

- DeZog DZRP 2.0 protocol implementation
- VSCode debugging integration via DeZog extension
- Remote debugging capabilities

## User Journeys

### Marcus - Retro Computing Hobbyist

**Persona:** Marcus, 45, software engineer by day, retro computing enthusiast by night. Backed the MicroBeast Kickstarter because it reminded him of his teenage years programming Z80 on a ZX Spectrum. He's competent but rusty - hasn't written assembly since the 80s. Programs in evenings, building a simple arcade game for MicroBeast.

---

### Journey 1: Debugging a Game Loop (Success Path)

**Opening Scene:** It's 9pm and Marcus has an hour before bed. His sprite movement code is misbehaving - the player character jumps erratically instead of moving smoothly. He's been adding `HALT` instructions and recompiling to trace execution, but it's tedious and he's losing momentum on the project.

**Rising Action:** Marcus opens BeastEm's new Breakpoints screen. He sets three breakpoints: one at his joystick read routine ($4000), one at sprite position update ($4080), and one at the screen draw ($4100). He runs the game and hits the first breakpoint. Steps through, watching registers. At the second breakpoint, he notices A contains garbage - not the joystick value he expected.

**Climax:** Stepping back through, Marcus spots it immediately. His joystick routine stores the result in A, but he's calling a utility function before the sprite update that clobbers A. He'd been staring at the code for days and missed it.

**Resolution:** Marcus adds a `PUSH AF` / `POP AF` around the utility call, recompiles, and his sprite glides smoothly. He grins, sets another breakpoint to verify the fix, and spends his remaining time adding a second sprite. The project feels fun again.

---

### Journey 2: Hunting Memory Corruption (Edge Case)

**Opening Scene:** Marcus's game crashes randomly - sometimes after 30 seconds, sometimes after two minutes. No pattern. The display corrupts and the emulator shows the Z80 in an invalid state. He's frustrated; print-debugging hasn't helped because the crash is never in the same place.

**Rising Action:** Marcus remembers his sprite table lives at $8000-$801F. He opens the Watchpoints tab and creates a watchpoint: address $8000, length 32 bytes, trigger on writes. He runs the game and waits, playing normally.

**Climax:** After 45 seconds, BeastEm breaks. The watchpoint triggered - something wrote to $8010. Marcus checks the PC: it's in his sound routine, which has a buffer at $7FF0. He checks his sound buffer size... 32 bytes. $7FF0 + $20 = $8010. His sound routine is writing one byte past its buffer into sprite memory.

**Resolution:** Marcus fixes the buffer bounds check, adds a watchpoint on $7FF0-$800F to verify his sound writes stay in bounds, and the random crashes are gone. A bug that could have taken hours of frustration was solved in minutes.

---

### Journey Requirements Summary

| Journey | Capabilities Required |
|---------|----------------------|
| Game Loop Debugging | Multiple breakpoints (3+), enable/disable individual breakpoints, step execution, register inspection |
| Memory Corruption | Watchpoints with address range, write-trigger detection, PC display on break, easy watchpoint creation |
| Both | Dedicated UI for managing breakpoints/watchpoints, clear status indication, quick add/remove workflow |

## Desktop App Specific Requirements

### Platform Support

| Platform | Status | Notes |
|----------|--------|-------|
| Windows | Supported | SDL2-based, identical behavior |
| Linux | Supported | SDL2-based, identical behavior |
| macOS | Supported | SDL2-based, identical behavior |

The breakpoint/watchpoint UI will render identically across all platforms using BeastEm's existing SDL2 rendering infrastructure. No platform-specific code paths required.

### System Integration

- **Keyboard:** Standard keyboard navigation for breakpoint/watchpoint management (consistent with existing BeastEm UI patterns)
- **File System:** No file I/O required - breakpoints and watchpoints are session-only
- **Existing Debugger:** Integrates with current single-step execution, register view, and memory inspection
- **Network:** Fully offline - no network dependency

### Data Persistence

**Session-only design:**
- Breakpoints and watchpoints exist only in memory
- Cleared when BeastEm exits
- No save/load functionality required for MVP
- Keeps implementation simple and focused

## Project Scoping & Phased Development

### MVP Strategy & Philosophy

**MVP Approach:** Problem-solving MVP - deliver core debugging value that makes Marcus's workflows possible.

**Design Rationale:** 8/8/2 limits are reasonable defaults chosen to avoid paging UI complexity while providing ample capacity for typical debugging sessions.

### MVP Feature Set (Phase 1)

**Core User Journeys Supported:**
- Game loop debugging (multiple breakpoints across code path)
- Memory corruption hunting (watchpoints on data regions)

**Must-Have Capabilities:**

| Capability | Details |
|------------|---------|
| Breakpoints | 8 user-configurable (address, optional bank, enabled flag) |
| Watchpoints | 8 user-configurable (address, range, type: read/write/both, enabled flag) |
| System Breakpoints | 2 hidden for future DeZog step-over/step-out support |
| UI | Dedicated screen with Breakpoints and Watchpoints tabs |
| Operations | Add, edit, delete breakpoints/watchpoints |
| Quick Toggle | Enable/disable directly from list without opening edit |
| Integration | Accurate triggering in emulation loop |

**Explicitly Not in MVP:**
- Jump-to-address from breakpoint list
- Persistence/save-load
- DeZog protocol integration

### Post-MVP Features

**Phase 2 (DeZog Foundation):**
- DeZog DZRP 2.0 protocol implementation
- System breakpoint utilization for step-over/step-out

**Phase 3 (External Integration):**
- VSCode debugging via DeZog extension
- Remote debugging capabilities

### Risk Mitigation Strategy

**Technical Risks:** Low - breakpoints/watchpoints are well-understood patterns; main risk is emulation loop performance impact (mitigated by efficient checking)

**Scope Risks:** Contained - clear 8/8/2 limits prevent feature creep; session-only eliminates persistence complexity

## Functional Requirements

### Breakpoint Management

- FR1: User can create a breakpoint at a specified memory address
- FR2: User can optionally associate a breakpoint with a specific memory bank
- FR3: User can enable or disable a breakpoint
- FR4: User can edit an existing breakpoint's address and bank
- FR5: User can delete a breakpoint
- FR6: User can quick-toggle a breakpoint's enabled state from the list view
- FR7: User can view all configured breakpoints and their states
- FR8: System limits user breakpoints to 8 maximum

### Watchpoint Management

- FR9: User can create a watchpoint at a specified memory address
- FR10: User can specify a watchpoint's range (number of bytes to monitor)
- FR11: User can specify watchpoint trigger type (read, write, or both)
- FR12: User can enable or disable a watchpoint
- FR13: User can edit an existing watchpoint's address, range, and type
- FR14: User can delete a watchpoint
- FR15: User can quick-toggle a watchpoint's enabled state from the list view
- FR16: User can view all configured watchpoints and their states
- FR17: System limits user watchpoints to 8 maximum

### Debugging UI

- FR18: User can access a dedicated breakpoints/watchpoints screen
- FR19: User can switch between Breakpoints and Watchpoints tabs
- FR20: User can see at a glance which breakpoints/watchpoints are enabled
- FR21: UI displays breakpoint addresses in hexadecimal format
- FR22: UI displays watchpoint addresses and ranges in hexadecimal format

### Emulation Integration

- FR23: System pauses emulation when PC matches an enabled breakpoint address
- FR24: System pauses emulation when memory access matches an enabled watchpoint
- FR25: System supports 2 hidden system breakpoints for programmatic use
- FR26: Breakpoint/watchpoint checking does not degrade emulation accuracy

## Non-Functional Requirements

### Performance

- NFR1: Breakpoint checking adds acceptable overhead only when breakpoints are enabled
- NFR2: Watchpoint checking adds acceptable overhead only when watchpoints are enabled
- NFR3: With no breakpoints/watchpoints enabled, emulation performance is unaffected
- NFR4: UI operations (add/edit/delete) complete without perceptible delay

### Accuracy

- NFR5: Breakpoints trigger on the exact instruction at the specified address (no misses, no false triggers)
- NFR6: Watchpoints detect all memory accesses within the monitored range (no misses)
- NFR7: Bank-specific breakpoints only trigger when the correct bank is paged in

### UI Consistency

- NFR8: Breakpoint/watchpoint UI follows existing BeastEm visual style and interaction patterns
- NFR9: Keyboard navigation consistent with other BeastEm screens
- NFR10: Hexadecimal address display matches existing debugger formatting conventions

### Integration

- NFR11: Feature integrates with existing single-step execution without conflicts
- NFR12: System breakpoints accessible programmatically for future DeZog integration
- NFR13: Breakpoint/watchpoint state queryable by other debugger components
