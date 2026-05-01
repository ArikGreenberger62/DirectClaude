// Project workspace — the heart of the POC
const { useState, useEffect, useRef, useMemo } = React;

const StatusBar = ({ runState, project, iter, maxIter, tokens, cost, elapsed }) => {
  const states = {
    idle:    { label: "Idle",                color: "var(--fg-2)",     dot: "" },
    running: { label: "Running",             color: "var(--cyan-400)", dot: "info" },
    waiting: { label: "Awaiting confirmation", color: "var(--magenta-400)", dot: "warn" },
    done:    { label: "Verified",            color: "#6EE7B7",          dot: "ok" },
    error:   { label: "Error",               color: "#FCA5A5",          dot: "err" },
  }[runState] || { label: "Idle", color: "var(--fg-2)", dot: "" };

  return (
    <div style={{
      display: "flex", alignItems: "center", gap: 18, padding: "8px 18px",
      borderBottom: "1px solid var(--border-1)",
      background: "rgba(7,6,15,0.5)", fontSize: 12,
    }}>
      <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
        <span className={"dot " + states.dot + (runState === "running" ? " pulse" : "")}/>
        <span style={{ color: states.color, fontWeight: 500 }}>{states.label}</span>
      </div>
      <span style={{ color: "var(--fg-4)" }}>·</span>
      <span style={{ color: "var(--fg-2)", fontFamily: "var(--font-mono)" }}>{project.id}</span>
      <span style={{ color: "var(--fg-4)" }}>·</span>
      <span style={{ color: "var(--fg-2)" }}>iter <span style={{ color: "var(--fg-0)", fontFamily: "var(--font-mono)" }}>{iter}/{maxIter}</span></span>
      <div style={{ flex: 1 }}/>
      <span style={{ color: "var(--fg-3)", display: "inline-flex", alignItems: "center", gap: 5 }}>
        <Icon.Clock size={11}/> {elapsed}
      </span>
      <span style={{ color: "var(--fg-3)" }}>tokens <span style={{ color: "var(--fg-1)", fontFamily: "var(--font-mono)" }}>{tokens.toLocaleString()}</span></span>
      <span style={{ color: "var(--fg-3)" }}>cost <span style={{ color: "var(--fg-1)", fontFamily: "var(--font-mono)" }}>${cost.toFixed(3)}</span></span>
    </div>
  );
};

const PipelineSteps = ({ steps }) => (
  <div style={{ display: "flex", gap: 6, padding: "10px 18px", borderBottom: "1px solid var(--border-1)" }}>
    {steps.map((s, i) => {
      const color =
        s.state === "done" ? "var(--ok)" :
        s.state === "active" ? "var(--cyan-400)" :
        s.state === "wait" ? "var(--magenta-400)" :
        s.state === "fail" ? "var(--err)" :
        "var(--fg-4)";
      const bg =
        s.state === "done" ? "rgba(74,222,128,0.08)" :
        s.state === "active" ? "rgba(79,215,255,0.10)" :
        s.state === "wait" ? "rgba(228,91,216,0.10)" :
        s.state === "fail" ? "rgba(248,113,113,0.10)" :
        "rgba(139,111,245,0.04)";
      return (
        <div key={s.label} style={{
          flex: 1, padding: "8px 10px", borderRadius: 8,
          background: bg, border: "1px solid " + (s.state === "idle" ? "var(--border-1)" : color.replace(")", ", 0.35)").replace("rgba(", "rgba(")),
          display: "flex", alignItems: "center", gap: 8,
        }}>
          <div style={{ color, display: "flex" }}>
            {s.state === "done" ? <Icon.Check size={13}/> :
             s.state === "fail" ? <Icon.X size={13}/> :
             s.state === "active" ? <span className="pulse" style={{ display: "flex" }}><Icon.CircleDot size={13}/></span> :
             s.state === "wait" ? <Icon.Alert size={13}/> :
             <Icon.CircleDot size={13}/>}
          </div>
          <div style={{ fontSize: 12, color: s.state === "idle" ? "var(--fg-3)" : "var(--fg-1)", fontWeight: 500 }}>
            {s.label}
          </div>
          {s.detail && <div style={{ fontSize: 11, color: "var(--fg-3)", fontFamily: "var(--font-mono)", marginLeft: "auto" }}>{s.detail}</div>}
        </div>
      );
    })}
  </div>
);

const TraceRow = ({ ev }) => {
  if (ev.kind === "session") return (
    <div className="trace-row event session">
      <div className="time">{ev.time}</div>
      <div className="gutter"><Icon.Sparkle size={12} style={{ color: "var(--cyan-400)" }}/></div>
      <div className="body">◆ {ev.text}</div>
    </div>
  );
  if (ev.kind === "meta") return (
    <div className="trace-row" style={{ color: "var(--fg-3)" }}>
      <div className="time">{ev.time}</div>
      <div className="gutter"/>
      <div className="body" style={{ color: "var(--fg-3)" }}>{ev.text}</div>
    </div>
  );
  if (ev.kind === "text") return (
    <div className="trace-row text">
      <div className="time">{ev.time}</div>
      <div className="gutter"><div className="dot" style={{ background: "var(--violet-400)" }}/></div>
      <div className="body">{ev.text}</div>
    </div>
  );
  if (ev.kind === "tool") return (
    <div className="trace-row tool event">
      <div className="time">{ev.time}</div>
      <div className="gutter"><Icon.Wrench size={12} style={{ color: "#FFD580" }}/></div>
      <div className="body">▶ <span className="name">{ev.name}</span> <span style={{ color: "var(--fg-2)", fontFamily: "var(--font-mono)" }}>{ev.arg}</span></div>
    </div>
  );
  if (ev.kind === "result") return (
    <div className="trace-row result">
      <div className="time">{ev.time}</div>
      <div className="gutter"/>
      <div className="body" style={{ whiteSpace: "pre-wrap" }}>└─ {ev.text}</div>
    </div>
  );
  if (ev.kind === "fix") return (
    <div className="trace-row fix event">
      <div className="time">{ev.time}</div>
      <div className="gutter"><Icon.Spark size={12} style={{ color: "var(--magenta-400)" }}/></div>
      <div className="body">{ev.text}</div>
    </div>
  );
  if (ev.kind === "success") return (
    <div className="trace-row success event">
      <div className="time">{ev.time}</div>
      <div className="gutter"><Icon.Check size={13} style={{ color: "var(--ok)" }}/></div>
      <div className="body">✔ {ev.text}</div>
    </div>
  );
  if (ev.kind === "error") return (
    <div className="trace-row error event">
      <div className="time">{ev.time}</div>
      <div className="gutter"><Icon.X size={13} style={{ color: "var(--err)" }}/></div>
      <div className="body">✘ {ev.text}</div>
    </div>
  );
  return null;
};

const ConfirmCard = ({ ev, onApprove, onReject, onEdit }) => {
  return (
    <div className="trace-row event anim-up" style={{ paddingTop: 14, paddingBottom: 14 }}>
      <div className="time">{ev.time}</div>
      <div className="gutter"><Icon.Alert size={13} style={{ color: "var(--magenta-400)" }}/></div>
      <div className="body" style={{ paddingRight: 16 }}>
        <div style={{
          background: "linear-gradient(180deg, rgba(228,91,216,0.10) 0%, rgba(228,91,216,0.03) 100%)",
          border: "1px solid rgba(228,91,216,0.35)",
          borderRadius: 12, padding: 16,
        }}>
          <div style={{ display: "flex", alignItems: "center", gap: 8, marginBottom: 6 }}>
            <span className="pill" style={{ background: "rgba(228,91,216,0.15)", borderColor: "rgba(228,91,216,0.4)", color: "#F5BEEF" }}>
              <Icon.Alert size={11}/> Confirmation required
            </span>
          </div>
          <div style={{ color: "var(--fg-0)", fontSize: 14, fontWeight: 500, marginBottom: 6, fontFamily: "var(--font-sans)" }}>
            {ev.title}
          </div>
          <div style={{ color: "var(--fg-2)", fontSize: 13, marginBottom: 12, fontFamily: "var(--font-sans)" }}>
            {ev.detail}
          </div>
          {ev.command && (
            <div style={{
              background: "rgba(7,6,15,0.65)", border: "1px solid var(--border-1)",
              borderRadius: 8, padding: "8px 12px", marginBottom: 12,
              fontFamily: "var(--font-mono)", fontSize: 12, color: "var(--fg-1)",
              display: "flex", alignItems: "center", gap: 8,
            }}>
              <Icon.Terminal size={12} style={{ color: "var(--fg-3)", flexShrink: 0 }}/>
              <span style={{ overflowX: "auto", whiteSpace: "nowrap", flex: 1 }}>$ {ev.command}</span>
            </div>
          )}
          <div style={{ display: "flex", gap: 8, fontFamily: "var(--font-sans)" }}>
            <button className="btn btn-confirm" onClick={onApprove}>
              <Icon.Check size={13}/> Approve & continue
            </button>
            <button className="btn btn-ghost" onClick={onEdit}>
              <Icon.Code size={13}/> Edit command
            </button>
            <button className="btn btn-danger" onClick={onReject}>
              <Icon.X size={13}/> Reject
            </button>
            <div style={{ flex: 1 }}/>
            <span style={{ color: "var(--fg-3)", fontSize: 11.5, alignSelf: "center" }}>
              <span className="kbd">⌘</span> <span className="kbd">↵</span> to approve
            </span>
          </div>
        </div>
      </div>
    </div>
  );
};

const TraceStream = ({ events, runState, onApprove, onReject }) => {
  const ref = useRef(null);
  useEffect(() => {
    if (ref.current) ref.current.scrollTop = ref.current.scrollHeight;
  }, [events.length]);

  return (
    <div ref={ref} style={{
      flex: 1, overflowY: "auto", paddingTop: 6, paddingBottom: 60,
      background: "rgba(7,6,15,0.4)",
    }}>
      {events.map((ev, i) =>
        ev.kind === "confirm"
          ? <ConfirmCard key={i} ev={ev} onApprove={onApprove} onReject={onReject} onEdit={onApprove}/>
          : <TraceRow key={i} ev={ev}/>
      )}
      {runState === "running" && (
        <div className="trace-row" style={{ paddingTop: 8 }}>
          <div className="time"/>
          <div className="gutter"><span className="pulse"><div className="dot info"/></span></div>
          <div className="body" style={{ color: "var(--fg-3)", fontStyle: "italic" }}>thinking…</div>
        </div>
      )}
    </div>
  );
};

const PromptComposer = ({ value, onChange, onRun, runState, onStop }) => {
  const ta = useRef(null);
  useEffect(() => {
    const el = ta.current; if (!el) return;
    el.style.height = "auto";
    el.style.height = Math.min(el.scrollHeight, 220) + "px";
  }, [value]);

  const isBusy = runState === "running" || runState === "waiting";

  return (
    <div style={{
      borderTop: "1px solid var(--border-1)",
      padding: 14,
      background: "linear-gradient(180deg, rgba(7,6,15,0.4) 0%, rgba(14,11,30,0.85) 100%)",
    }}>
      <div style={{
        border: "1px solid var(--border-2)", borderRadius: 14,
        background: "rgba(7,6,15,0.6)", padding: 12,
        transition: "border-color .15s",
      }}>
        <textarea
          ref={ta}
          value={value}
          onChange={e => onChange(e.target.value)}
          placeholder="Describe what to build, fix, or verify…  e.g. 'flash 09-sms-modem and verify EG915N power-on trace on COM7'"
          rows={2}
          onKeyDown={e => {
            if ((e.metaKey || e.ctrlKey) && e.key === "Enter") { e.preventDefault(); onRun(); }
          }}
          style={{
            width: "100%", background: "transparent", border: 0, outline: 0,
            resize: "none", color: "var(--fg-0)", fontFamily: "var(--font-sans)",
            fontSize: 14, lineHeight: 1.55, minHeight: 44,
          }}
        />
        <div style={{
          display: "flex", alignItems: "center", gap: 8, paddingTop: 8,
          borderTop: "1px dashed var(--border-1)", marginTop: 6,
        }}>
          <button className="btn btn-ghost btn-sm" title="Pin a file"><Icon.Pin size={12}/> Attach datasheet</button>
          <button className="btn btn-ghost btn-sm" title="Set task type"><Icon.Hash size={12}/> code, build, test</button>
          <button className="btn btn-ghost btn-sm" title="Continue"><Icon.Folder size={12}/> --continue</button>
          <div style={{ flex: 1 }}/>
          <span style={{ color: "var(--fg-3)", fontSize: 11.5, marginRight: 6 }}>
            <span className="kbd">⌘</span> <span className="kbd">↵</span> to run
          </span>
          {isBusy ? (
            <button className="btn btn-danger" onClick={onStop}>
              <Icon.Stop size={12}/> Stop
            </button>
          ) : (
            <button className="btn btn-primary" onClick={onRun} disabled={!value.trim()}>
              <Icon.Play size={12}/> Run
            </button>
          )}
        </div>
      </div>
    </div>
  );
};

const SidePanel = ({ project, onBack }) => {
  return (
    <aside style={{
      width: 280, borderLeft: "1px solid var(--border-1)",
      background: "rgba(14,11,30,0.5)",
      display: "flex", flexDirection: "column", overflow: "hidden",
    }}>
      <div style={{ padding: "16px 18px", borderBottom: "1px solid var(--border-1)" }}>
        <div style={{ color: "var(--fg-3)", fontSize: 11, letterSpacing: "0.08em", textTransform: "uppercase", marginBottom: 6 }}>Project</div>
        <div style={{ fontSize: 15, fontWeight: 500, color: "var(--fg-0)" }}>{project.name}</div>
        <div style={{ fontSize: 12, color: "var(--fg-3)", fontFamily: "var(--font-mono)", marginTop: 2 }}>{project.mcu}</div>
      </div>

      <div style={{ padding: "14px 18px", borderBottom: "1px solid var(--border-1)" }}>
        <div style={{ color: "var(--fg-3)", fontSize: 11, letterSpacing: "0.08em", textTransform: "uppercase", marginBottom: 8 }}>Hardware</div>
        <div style={{ display: "flex", flexDirection: "column", gap: 6, fontSize: 12.5 }}>
          <div style={{ display: "flex", justifyContent: "space-between" }}>
            <span style={{ color: "var(--fg-2)" }}><Icon.Cable size={11} style={{ marginRight: 6, verticalAlign: -1 }}/>ST-LINK</span>
            <span className="pill ok" style={{ padding: "1px 7px", fontSize: 10.5 }}><span className="dot ok"/>connected</span>
          </div>
          <div style={{ display: "flex", justifyContent: "space-between" }}>
            <span style={{ color: "var(--fg-2)" }}><Icon.Terminal size={11} style={{ marginRight: 6, verticalAlign: -1 }}/>COM7</span>
            <span style={{ color: "var(--fg-3)", fontFamily: "var(--font-mono)" }}>115200</span>
          </div>
          <div style={{ display: "flex", justifyContent: "space-between" }}>
            <span style={{ color: "var(--fg-2)" }}>SN</span>
            <span style={{ color: "var(--fg-3)", fontFamily: "var(--font-mono)", fontSize: 11 }}>002C…3939</span>
          </div>
        </div>
      </div>

      <div style={{ padding: "14px 18px", borderBottom: "1px solid var(--border-1)" }}>
        <div style={{ color: "var(--fg-3)", fontSize: 11, letterSpacing: "0.08em", textTransform: "uppercase", marginBottom: 8 }}>Skills loaded</div>
        <div style={{ display: "flex", flexWrap: "wrap", gap: 6 }}>
          {["stm32", "modem", "embedded-general/coding", "embedded-general/testing"].map(s => (
            <span key={s} className="pill" style={{ fontFamily: "var(--font-mono)", fontSize: 10.5 }}>{s}</span>
          ))}
        </div>
      </div>

      <div style={{ padding: "14px 18px", borderBottom: "1px solid var(--border-1)", flex: 1, overflow: "auto" }}>
        <div style={{ color: "var(--fg-3)", fontSize: 11, letterSpacing: "0.08em", textTransform: "uppercase", marginBottom: 8 }}>STATE.md</div>
        <div style={{
          background: "rgba(7,6,15,0.5)", border: "1px solid var(--border-1)",
          borderRadius: 8, padding: 10,
          fontFamily: "var(--font-mono)", fontSize: 11.5, color: "var(--fg-2)",
          whiteSpace: "pre-wrap", lineHeight: 1.6,
        }}>
{`# 09-sms-modem — STATE

## Status
Build clean. Flash + trace verify pending.

## Key facts
- UART2 → modem (incl. CTS/RTS)
- MC60_GNSS_PWR enables modem rail
- MC60_PWRKEY pulse 1100 ms turns it on
- AT cmd ref: EG915N v1.2

## Next step
RESUME: flash and read trace on COM7.`}
        </div>
      </div>

      <div style={{ padding: 14, display: "flex", gap: 8 }}>
        <button className="btn btn-ghost btn-sm" onClick={onBack} style={{ flex: 1 }}>
          ← All projects
        </button>
      </div>
    </aside>
  );
};

const ProjectWorkspace = ({ project, onBack, onLogout, mockState, setMockState }) => {
  const [prompt, setPrompt] = useState("flash 09-sms-modem and verify the EG915N power-on trace on COM7");
  const [events, setEvents] = useState([]);
  const [runState, setRunState] = useState("idle"); // idle | running | waiting | done | error
  const [iter, setIter] = useState(0);
  const [tokens, setTokens] = useState(0);
  const [cost, setCost] = useState(0);
  const [elapsed, setElapsed] = useState("0s");
  const elapsedRef = useRef(0);
  const playRef = useRef({ idx: 0, paused: false });

  // Pipeline steps mirror the script
  const pipeline = useMemo(() => {
    const seenTools = events.filter(e => e.kind === "tool").map(e => (e.arg || "") + " " + (e.name || ""));
    const isDone = runState === "done";
    const waiting = events.length > 0 && events[events.length - 1] && events[events.length - 1].kind === "confirm";

    const seen = (kw) => seenTools.some(t => t.toLowerCase().includes(kw));
    const built = seen("cmake --build");
    const probe = seen("-l st");
    const flashed = seen("-w build");
    const traced = seen("trace.py");

    return [
      { label: "Plan",    state: events.length > 0 ? "done" : "idle", detail: "" },
      { label: "Build",   state: built ? "done" : (events.length > 4 ? "active" : "idle"), detail: built ? "38.4 KB" : "" },
      { label: "Flash",   state: flashed ? "done" : (waiting && !flashed ? "wait" : (probe ? "active" : "idle")), detail: flashed ? "OK" : (waiting ? "approve" : "") },
      { label: "Trace",   state: traced ? "done" : (flashed && !traced ? "active" : "idle"), detail: traced ? "COM7" : "" },
      { label: "Verify",  state: isDone ? "done" : "idle", detail: isDone ? "✓" : "" },
    ];
  }, [events, runState]);

  // Driver — replays SAMPLE_RUN
  useEffect(() => {
    if (runState !== "running") return;
    let cancelled = false;
    const tick = () => {
      if (cancelled) return;
      const i = playRef.current.idx;
      if (i >= SAMPLE_RUN.length) { setRunState("done"); return; }
      const ev = SAMPLE_RUN[i];
      setEvents(prev => [...prev, ev]);
      setTokens(t => t + 80 + Math.floor(Math.random() * 220));
      setCost(c => c + 0.005 + Math.random() * 0.012);
      if (ev.kind === "tool") setIter(n => n + 1);
      playRef.current.idx = i + 1;

      if (ev.kind === "confirm") { setRunState("waiting"); return; }
      if (ev.kind === "success") { setRunState("done"); return; }
      const delay = ev.kind === "result" ? 320 : ev.kind === "tool" ? 480 : ev.kind === "text" ? 700 : 260;
      setTimeout(tick, delay);
    };
    const id = setTimeout(tick, 200);
    return () => { cancelled = true; clearTimeout(id); };
  }, [runState]);

  // Elapsed timer
  useEffect(() => {
    if (runState !== "running" && runState !== "waiting") return;
    const id = setInterval(() => {
      elapsedRef.current += 1;
      const s = elapsedRef.current;
      setElapsed(s < 60 ? s + "s" : Math.floor(s/60) + "m " + (s%60) + "s");
    }, 1000);
    return () => clearInterval(id);
  }, [runState]);

  // React to Tweaks-driven mock state
  useEffect(() => {
    if (!mockState || mockState === "auto") return;
    if (mockState === "idle") {
      setEvents([]); setRunState("idle"); setIter(0); setTokens(0); setCost(0); setElapsed("0s");
      elapsedRef.current = 0; playRef.current.idx = 0;
    } else if (mockState === "running") {
      setEvents([]); playRef.current.idx = 0; elapsedRef.current = 0;
      setIter(0); setTokens(0); setCost(0); setElapsed("0s");
      setRunState("running");
    } else if (mockState === "waiting") {
      // pre-fill up to confirm
      const upto = SAMPLE_RUN.findIndex(e => e.kind === "confirm");
      const slice = SAMPLE_RUN.slice(0, upto + 1);
      setEvents(slice);
      playRef.current.idx = upto + 1;
      setIter(slice.filter(e => e.kind === "tool").length);
      setTokens(2400); setCost(0.083); setElapsed("28s"); elapsedRef.current = 28;
      setRunState("waiting");
    } else if (mockState === "done") {
      setEvents(SAMPLE_RUN);
      playRef.current.idx = SAMPLE_RUN.length;
      setIter(SAMPLE_RUN.filter(e => e.kind === "tool").length);
      setTokens(8420); setCost(0.184); setElapsed("1m 10s"); elapsedRef.current = 70;
      setRunState("done");
    }
  }, [mockState]);

  const run = () => {
    setEvents([]); playRef.current.idx = 0; elapsedRef.current = 0;
    setIter(0); setTokens(0); setCost(0); setElapsed("0s");
    setRunState("running");
    setMockState && setMockState("auto");
  };
  const stop = () => { setRunState("idle"); };
  const approve = () => {
    setRunState("running");
    setTimeout(() => {
      // continue ticking from where we left off
      const tick = () => {
        const i = playRef.current.idx;
        if (i >= SAMPLE_RUN.length) { setRunState("done"); return; }
        const ev = SAMPLE_RUN[i];
        setEvents(prev => [...prev, ev]);
        setTokens(t => t + 80 + Math.floor(Math.random() * 220));
        setCost(c => c + 0.005 + Math.random() * 0.012);
        if (ev.kind === "tool") setIter(n => n + 1);
        playRef.current.idx = i + 1;
        if (ev.kind === "confirm") { setRunState("waiting"); return; }
        if (ev.kind === "success") { setRunState("done"); return; }
        const delay = ev.kind === "result" ? 320 : ev.kind === "tool" ? 480 : ev.kind === "text" ? 700 : 260;
        setTimeout(tick, delay);
      };
      tick();
    }, 80);
  };
  const reject = () => { setRunState("error"); setEvents(prev => [...prev, { kind: "error", time: new Date().toTimeString().slice(0,8), text: "User rejected. Stopping run." }]); };

  return (
    <div style={{ flex: 1, display: "flex", flexDirection: "column", overflow: "hidden" }}>
      {/* Top bar */}
      <header style={{
        display: "flex", alignItems: "center", justifyContent: "space-between",
        padding: "12px 22px", borderBottom: "1px solid var(--border-1)",
        background: "rgba(14,11,30,0.6)", backdropFilter: "blur(8px)",
      }}>
        <div style={{ display: "flex", alignItems: "center", gap: 14 }}>
          <button className="btn btn-ghost btn-sm" onClick={onBack}>
            <Icon.ChevronRight size={12} style={{ transform: "rotate(180deg)" }}/> Projects
          </button>
          <div style={{ width: 1, height: 20, background: "var(--border-1)" }}/>
          <div className="logo">
            <div className="logo-mark"/>
            <span style={{ fontSize: 14 }}>{project.name}</span>
          </div>
        </div>
        <div style={{ display: "flex", alignItems: "center", gap: 10 }}>
          <span className="pill"><Icon.Cpu size={11}/> {project.mcu}</span>
          <span className="pill ok"><span className="dot ok"/>ST-LINK · COM7</span>
          <button className="btn btn-ghost btn-sm"><Icon.Settings size={14}/></button>
          <button className="btn btn-ghost btn-sm" onClick={onLogout}><Icon.Logout size={14}/></button>
        </div>
      </header>

      <StatusBar runState={runState} project={project} iter={iter} maxIter={10} tokens={tokens} cost={cost} elapsed={elapsed}/>
      <PipelineSteps steps={pipeline}/>

      {/* Main split */}
      <div style={{ flex: 1, display: "flex", overflow: "hidden" }}>
        <main style={{ flex: 1, display: "flex", flexDirection: "column", overflow: "hidden" }}>
          {events.length === 0 ? (
            <EmptyTrace onUseSample={run}/>
          ) : (
            <TraceStream events={events} runState={runState} onApprove={approve} onReject={reject}/>
          )}
          <PromptComposer value={prompt} onChange={setPrompt} onRun={run} runState={runState} onStop={stop}/>
        </main>
        <SidePanel project={project} onBack={onBack}/>
      </div>
    </div>
  );
};

const EmptyTrace = ({ onUseSample }) => (
  <div style={{ flex: 1, display: "flex", alignItems: "center", justifyContent: "center", padding: 32 }}>
    <div style={{ maxWidth: 520, textAlign: "center" }}>
      <div style={{
        width: 56, height: 56, margin: "0 auto 18px",
        borderRadius: 14, background: "rgba(139,111,245,0.10)",
        border: "1px solid var(--border-2)",
        display: "flex", alignItems: "center", justifyContent: "center",
        color: "var(--violet-200)",
      }}>
        <Icon.Bolt size={22}/>
      </div>
      <div style={{ fontFamily: "var(--font-display)", fontSize: 22, fontWeight: 600, color: "var(--fg-0)", marginBottom: 8, letterSpacing: "-0.01em" }}>
        Ready when you are
      </div>
      <div style={{ color: "var(--fg-2)", fontSize: 14, marginBottom: 22, lineHeight: 1.55 }}>
        Type a prompt below to ask CCoreAi to build, flash, or fix this project.
        Trace and confirmations will appear here as the run progresses.
      </div>
      <div style={{ display: "flex", gap: 8, justifyContent: "center", flexWrap: "wrap" }}>
        <button className="btn btn-ghost btn-sm" onClick={onUseSample}>
          <Icon.Play size={11}/> Replay sample run
        </button>
        <button className="btn btn-ghost btn-sm">
          <Icon.Code size={11}/> Show last STATE.md
        </button>
      </div>
    </div>
  </div>
);

window.ProjectWorkspace = ProjectWorkspace;
