---
title: "Network Automation: Declarative State, NETCONF/YANG, gNMI and Safe Changes"
track: networking
module: Operations and Automation
moduleOrder: 11
order: 3
level: intermediate
summary: "From typing commands to managing intent: declarative versus imperative configuration, idempotency and drift (with a working Linux reconciler), validation, dry runs and automatic rollback, the model-driven stack (YANG, NETCONF, RESTCONF, gNMI), source-of-truth and CI practices, and the failure modes of automating at scale."
prerequisites:
  - "Network Monitoring: SNMP, Flow Export and Streaming Telemetry"
  - "Linux Network Labs: Namespaces, veth, Bridges, VLANs, tc and nftables"
sources:
  - label: RFC 6241 — Network Configuration Protocol (NETCONF)
    url: https://www.rfc-editor.org/rfc/rfc6241
  - label: RFC 7950 — The YANG 1.1 Data Modeling Language
    url: https://www.rfc-editor.org/rfc/rfc7950
  - label: RFC 8040 — RESTCONF Protocol
    url: https://www.rfc-editor.org/rfc/rfc8040
  - label: RFC 8343 — A YANG Data Model for Interface Management (ietf-interfaces)
    url: https://www.rfc-editor.org/rfc/rfc8343
  - label: gNMI specification (OpenConfig)
    url: https://github.com/openconfig/reference/blob/master/rpc/gnmi/gnmi-specification.md
  - label: OpenConfig — vendor-neutral YANG models
    url: https://www.openconfig.net/
  - label: ip(8) and the JSON output (-j) of iproute2
    url: https://man7.org/linux/man-pages/man8/ip.8.html
  - label: Limoncelli, Chalup, Hogan — The Practice of Cloud System Administration (infrastructure as code chapters)
---

Configuring a network by typing commands into each device works for ten devices and fails at a thousand: it is slow, error-prone, unrepeatable, and leaves no record of why a device differs from its neighbours. **Network automation** replaces manual commands with software that describes what the network should look like and makes devices match that description. It is less about scripting and more about a change in model: from *procedures* to *state*.

## Imperative versus declarative

| | Imperative ("do these steps") | Declarative ("make it look like this") |
|---|---|---|
| Example | `ip addr add 10.0.0.1/24 dev uplink` | desired state: `uplink` has address `10.0.0.1/24` |
| Run twice | fails or duplicates | no change the second time |
| Knows about existing state | no | yes: compares and changes only the difference |
| Drift (someone changed the device) | invisible | detected and repaired |
| Dry run | rarely possible | natural: print the plan |

The central property is **idempotency**: applying the same desired state repeatedly yields the same result and makes no change once it has been reached. It makes automation safe to re-run after a failure, and makes "what changed?" answerable.

## A working reconciler

The program below is a tiny declarative configurator for a Linux "router". It does what every real tool (Ansible modules, Terraform providers, Kubernetes controllers, network controllers) does in essence: **validate** the desired state, **read** the actual state, compute a **plan** (the minimal list of changes, each paired with its undo), and **apply** it, optionally with a **health check and automatic rollback**. It reads actual state as JSON from `ip -j` (the same structured output real automation uses instead of scraping text) and writes changes with `ip`.

{{include networking/automation/desired.json}}

{{include networking/automation/reconcile.py}}

{{include networking/automation/lab.sh}}

{{run networking/automation :: unshare -Urnm sh lab.sh 2>&1}}

Walk through the output:

1. **Dry run (`--check`)** prints what *would* change on a fresh device (seven actions) without touching it: review before applying. This is the "plan" step of Terraform or `--check` mode of Ansible.
2. **Apply** performs those seven changes. **Applying again changes nothing**: idempotent.
3. The **imperative** command run a second time fails (`Address already assigned`): scripts built from raw commands break on re-runs and need hand-written guards for every step.
4. **Drift**: after someone manually changes the MTU, adds a stray address and deletes the route, the dry run lists exactly the three corrections, and the real run repairs them. With `--strict` the tool also removes **unexpected** things not in the desired state; without it, it only adds or fixes (the "converge but tolerate extras" mode). Choosing strictness is a real design decision: strict is safer for consistency, but deletes anything an operator added for a legitimate reason.
5. **Validation first**: a malformed address (`10.0.0.300/24`) is rejected before any change is made. Real systems validate against a **schema** (YANG, JSON Schema) so errors are caught at review time, not at 3 a.m. on a router.
6. **Safe change with rollback**: the new desired state moves the interface to a different subnet, which cuts the path to the neighbour. After applying, the tool runs a **health check** (a ping to the neighbour); it fails, so the tool **undoes every action in reverse order** and the device returns to a working state, which the final dry run confirms. This is the pattern behind NETCONF's *confirmed commit* and the "reload in 5" safety on classic router CLIs: apply the change, then keep it only if you can still reach the device.

## The model-driven stack

CLI scraping (Netmiko, expect scripts, regular expressions over `show` output) is fragile: output formats change between software versions and vendors. The industry moved to **structured, model-driven management**:

- **YANG** (RFC 7950) is a data modeling language that describes configuration and state as a typed tree: *containers*, *lists* with keys, *leaves* with types and constraints. A model defines what is valid; tools generate code and validation from it. Three families of models exist: **IETF** (standard, such as `ietf-interfaces`, RFC 8343), **OpenConfig** (vendor-neutral models written by operators for multi-vendor networks) and **vendor-native** models.
- **NETCONF** (RFC 6241) is the management protocol: an XML-based RPC over SSH (TCP port 830). Its key concept is **datastores**: `running` (the active configuration), optionally `candidate` (a scratch copy you edit and then **commit** atomically, or **discard**) and `startup`. Operations: `<get-config>`, `<get>` (config and state), `<edit-config>`, `<commit>` (with *confirmed commit*: revert automatically unless confirmed within a timeout), `<lock>`, `<validate>`.
- **RESTCONF** (RFC 8040) exposes the same YANG data over HTTPS with JSON or XML (`GET /restconf/data/ietf-interfaces:interfaces`), easy to use from any HTTP client.
- **gNMI** (gRPC Network Management Interface) uses gRPC with Protobuf for `Get`, `Set` and, importantly, `Subscribe` (streaming telemetry; see the monitoring note), and is the model-driven interface favoured for large-scale operation and telemetry.

A NETCONF request that sets an interface's description and enables it looks like this (reference example in the style of RFC 8343 and RFC 6241; it was not run against a device in this environment):

```xml
<rpc message-id="101" xmlns="urn:ietf:params:xml:ns:netconf:base:1.0">
  <edit-config>
    <target><candidate/></target>
    <config>
      <interfaces xmlns="urn:ietf:params:xml:ns:yang:ietf-interfaces">
        <interface>
          <name>eth0</name>
          <description>uplink to core</description>
          <enabled>true</enabled>
        </interface>
      </interfaces>
    </config>
  </edit-config>
</rpc>
<!-- then <commit><confirmed/><confirm-timeout>120</confirm-timeout></commit>; and a final <commit/> keeps it -->
```

Compared with the reconciler above: the **desired state is data** (a YANG-modeled document), the **device validates** it against the model, and **transactions** (candidate plus commit, confirmed commit) give atomicity and rollback that a script of CLI commands can only approximate.

## Architecture of an automation system

1. **Source of truth**: a database of intent: what devices, interfaces, VLANs, addresses and policies *should* exist (NetBox or another IPAM/DCIM, or structured files in Git). Everything else is *derived* from it.
2. **Templates/models**: the source of truth plus **templates** (Jinja2 or similar) or typed models generate each device's configuration. Variables hold per-device values; logic holds the design.
3. **Orchestration**: a tool applies changes: **Ansible** (agentless, YAML playbooks with network modules), **Terraform**-style planners, Python frameworks (Nornir), vendor controllers.
4. **Verification**: pre-checks (does the data validate?), a plan or diff, post-checks (are BGP sessions up? do routes match? does a test flow pass?).
5. **Telemetry feedback**: streaming telemetry and monitoring confirm the network behaves as intended and detect drift.
6. **Pipeline**: changes are made by **pull request**: reviewed, tested in CI against a virtual lab or emulator, then rolled out in stages (canary devices first), with the Git history as the audit trail.

## Safe-change principles

- **Validate before you touch**: schema and sanity checks (no overlapping prefixes, MTU matches on both ends of a link, route targets are reachable).
- **Plan, show the diff, then apply**; require human approval for risky changes.
- **Small blast radius**: change one device or one rack first (canary), verify, then widen. Automation makes a mistake instantly global.
- **Health checks and automatic rollback** (confirmed commit, "ping the gateway, else undo").
- **Idempotent and re-runnable**; **locking** so two automations cannot edit one device at once.
- **Order matters**: some changes must be sequenced (add the new path before removing the old).
- **Test in emulation**: Linux namespaces (as here), containerlab or vendor virtual routers, and offline configuration analysers (such as Batfish) catch mistakes before production.
- **Secrets** (device credentials, SNMPv3 keys) belong in a vault, never in Git or logs.
- **Observability of the automation itself**: log every change with who, what and why; alert on failures and on drift.

## Failure modes

| Failure | Cause | Defence |
|---|---|---|
| A bad change reaches every device at once | no staging; automation scales mistakes | canary rollout, approvals, rate limits |
| Device left half-configured | failure midway through a sequence of commands | transactions (candidate/commit) or undo-able plans |
| Automation "fights" an operator | manual change reverted by the next run, or opposite | one source of truth; policy on manual changes; drift alerts |
| Passes in CI, fails on real hardware | emulated behavior differs; vendor-specific features | test on representative hardware, vendor models |
| Locked out of the device | change removed the management path | health check through the management path, confirmed commit |
| Credentials leaked | secrets in repository or logs | vault, scoped credentials, rotation |
| Silent partial coverage | some devices unreachable or skipped | report the full result per device; fail loudly |

## Key takeaways

1. Automation manages state, not steps: declare the desired configuration and let a tool compute and apply the difference.
2. Idempotency, validation, dry-run plans and drift detection are what make automation safe to run repeatedly.
3. Every change should be reversible: a health check with automatic rollback (or NETCONF confirmed commit) protects against locking yourself out.
4. The model-driven stack replaces CLI scraping: YANG models, NETCONF/RESTCONF for configuration, gNMI for configuration and streaming telemetry.
5. Keep a source of truth, generate configuration from it, review changes in Git, test in emulation, roll out in stages.
6. Automation multiplies both productivity and mistakes: limit the blast radius and verify after every change.
