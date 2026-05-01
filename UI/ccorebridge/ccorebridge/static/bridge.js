// CCoreBridge — runtime client for the FastAPI bridge.
//
// The original /UI/CCoreAi.html prototype replayed SAMPLE_RUN against the
// React tree. The bridge-served version of the SPA loads this file first
// to expose a `window.CCB` helper that the rewritten components use.
//
// All transport details (token, base URL, WS URL) live here. Components
// only see CCB.{auth,listProjects,startRun,openRunStream,confirm,stop}.

(function () {
  const TOKEN_KEY = "ccb.token";
  const HOST = location.host || "127.0.0.1:8765";
  const ORIGIN = location.protocol + "//" + HOST;
  const WS_ORIGIN = (location.protocol === "https:" ? "wss:" : "ws:") + "//" + HOST;

  function getToken() {
    return localStorage.getItem(TOKEN_KEY) || "";
  }
  function setToken(t) {
    if (t) localStorage.setItem(TOKEN_KEY, t);
    else localStorage.removeItem(TOKEN_KEY);
  }

  async function jsonFetch(path, opts) {
    opts = opts || {};
    const headers = Object.assign({}, opts.headers || {}, {
      "Authorization": "Bearer " + getToken(),
    });
    if (opts.body && typeof opts.body !== "string") {
      headers["Content-Type"] = "application/json";
      opts.body = JSON.stringify(opts.body);
    }
    const r = await fetch(ORIGIN + path, Object.assign({}, opts, { headers }));
    const text = await r.text();
    let parsed;
    try { parsed = text ? JSON.parse(text) : null; } catch { parsed = text; }
    if (!r.ok) {
      const err = new Error((parsed && parsed.detail) || ("HTTP " + r.status));
      err.status = r.status;
      err.body = parsed;
      throw err;
    }
    return parsed;
  }

  async function health() {
    // /api/health is unauthenticated.
    const r = await fetch(ORIGIN + "/api/health");
    if (!r.ok) throw new Error("bridge offline");
    return r.json();
  }

  async function login(token) {
    setToken(token);
    try {
      await jsonFetch("/api/auth", { method: "POST", body: { token } });
      return true;
    } catch (e) {
      setToken("");
      throw e;
    }
  }

  function logout() { setToken(""); }
  function hasToken() { return !!getToken(); }

  async function listProjects() {
    return jsonFetch("/api/projects");
  }

  async function getProject(id) {
    return jsonFetch("/api/projects/" + encodeURIComponent(id));
  }

  async function startRun(body) {
    return jsonFetch("/api/run", { method: "POST", body });
  }

  async function confirm(runId, id, approve, reason) {
    return jsonFetch("/api/run/" + encodeURIComponent(runId) + "/confirm", {
      method: "POST",
      body: { id, action: approve ? "approve" : "reject", approve, reason },
    });
  }

  async function stopRun(runId) {
    return jsonFetch("/api/run/" + encodeURIComponent(runId) + "/stop", { method: "POST" });
  }

  // Open a WebSocket subscribed to a run. Returns an object with .close() and
  // delivers parsed events to onEvent. Auto-reconnect on transient drop.
  function openRunStream(runId, onEvent, onState) {
    let ws = null;
    let closed = false;
    let backoff = 600;

    function notifyState(s) { try { onState && onState(s); } catch (_) {} }

    function connect() {
      if (closed) return;
      const url = WS_ORIGIN + "/ws/run/" + encodeURIComponent(runId) +
                  "?token=" + encodeURIComponent(getToken());
      ws = new WebSocket(url);
      notifyState("connecting");
      ws.onopen = () => { backoff = 600; notifyState("open"); };
      ws.onmessage = (m) => {
        let evt; try { evt = JSON.parse(m.data); } catch { return; }
        try { onEvent(evt); } catch (e) { console.error("onEvent threw", e); }
      };
      ws.onerror = () => {};
      ws.onclose = (e) => {
        notifyState("closed");
        if (closed) return;
        if (e && (e.code === 4401 || e.code === 4404)) return;
        setTimeout(connect, backoff);
        backoff = Math.min(backoff * 2, 8000);
      };
    }

    connect();

    return {
      send(obj) {
        if (ws && ws.readyState === 1) ws.send(JSON.stringify(obj));
      },
      close() {
        closed = true;
        try { if (ws) ws.close(); } catch (_) {}
      },
    };
  }

  // Translate a server event into the SAMPLE_RUN-shaped event the existing
  // TraceRow/ConfirmCard components consume. Returns one or more events, or
  // null when the event should not appear in the trace pane.
  function toTraceEvents(evt) {
    const time = evt.time || (new Date()).toTimeString().slice(0, 8);
    const t = evt.type;
    if (t === "session") {
      return [{ kind: "session", time, text: evt.text || ("Session started · model: " + (evt.model || "?")) }];
    }
    if (t === "run.start") {
      const proj = evt.project ? "  ·  Continue: " + evt.project : "";
      return [{
        kind: "meta",
        time,
        text: "Run " + evt.run_id + proj,
      }];
    }
    if (t === "run.end") {
      const ok = evt.exit_code === 0;
      return [{
        kind: ok ? "success" : "error",
        time,
        text: ok
          ? ("Done · " + (evt.turns ?? "?") + " turns" + (evt.cost_usd ? " · cost $" + evt.cost_usd.toFixed(3) : ""))
          : ("Run ended with code " + evt.exit_code),
      }];
    }
    if (t === "text") {
      const stream = evt.stream || "assistant";
      if (stream === "stderr") {
        return [{ kind: "meta", time, text: "[stderr] " + (evt.text || "") }];
      }
      return [{ kind: "text", time, text: evt.text || "" }];
    }
    if (t === "tool") {
      return [{
        kind: "tool",
        time,
        name: evt.name || "?",
        arg: summariseInput(evt.name, evt.input || {}),
      }];
    }
    if (t === "result") {
      return [{ kind: "result", time, text: (evt.text || "").slice(0, 4000) }];
    }
    if (t === "result.final") {
      // Suppress — run.end already announces completion. Append text only.
      const txt = evt.text;
      if (!txt) return null;
      return [{ kind: "text", time, text: txt }];
    }
    if (t === "meta") {
      return null; // hide raw passthrough events
    }
    if (t === "error") {
      return [{ kind: "error", time, text: evt.message || "(error)" }];
    }
    if (t === "confirm.request") {
      return [{
        kind: "confirm",
        time,
        id: evt.id,
        title: evt.title || "Confirmation required",
        detail: evt.detail || "",
        command: evt.command || "",
        danger: evt.category === "destructive",
        category: evt.category,
        tool_use_id: evt.tool_use_id,
      }];
    }
    if (t === "confirm.resolved") {
      return null; // UI removes the confirm card itself when answered
    }
    return null;
  }

  function summariseInput(name, inp) {
    if (!inp) return "";
    if (name === "Bash") return (inp.command || "").slice(0, 200);
    if (name === "Read" || name === "View") return inp.file_path || inp.path || "";
    if (name === "Edit" || name === "Write" || name === "MultiEdit") return inp.file_path || inp.path || "";
    if (name === "Grep") return '"' + (inp.pattern || "") + '"';
    if (name === "Glob") return inp.pattern || "";
    return JSON.stringify(inp).slice(0, 200);
  }

  window.CCB = {
    health, login, logout, hasToken, getToken, setToken,
    listProjects, getProject,
    startRun, confirm, stopRun,
    openRunStream, toTraceEvents,
    ORIGIN, WS_ORIGIN,
  };
})();
