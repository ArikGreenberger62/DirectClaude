// Project Selector screen
const ProjectSelector = ({ projects, onOpen, onLogout, onNew, onDelete, user }) => {
  const [q, setQ] = React.useState("");
  const filtered = projects.filter(p =>
    !q || p.name.toLowerCase().includes(q.toLowerCase()) ||
    p.summary.toLowerCase().includes(q.toLowerCase())
  );

  const statusPill = (status, label) => {
    const cls =
      status === "verified" ? "ok" :
      status === "build-clean" ? "warn" :
      status === "running" ? "run" : "";
    return <span className={"pill " + cls}><span className="dot"/>{label}</span>;
  };

  return (
    <div style={{ flex: 1, display: "flex", flexDirection: "column", overflow: "hidden" }}>
      {/* Top bar */}
      <header style={{
        display: "flex", alignItems: "center", justifyContent: "space-between",
        padding: "14px 28px", borderBottom: "1px solid var(--border-1)",
        background: "rgba(14,11,30,0.6)", backdropFilter: "blur(8px)",
      }}>
        <div className="logo">
          <div className="logo-mark"/>
          <span style={{ fontSize: 16 }}>CCore<span className="accent" style={{
            background: "linear-gradient(90deg, #A989FF 0%, #E45BD8 70%)",
            WebkitBackgroundClip: "text", backgroundClip: "text", color: "transparent",
          }}>Ai</span></span>
        </div>
        <div style={{ display: "flex", alignItems: "center", gap: 10 }}>
          <span className="pill ok"><span className="dot ok"/>ST-LINK · STM32H573</span>
          <span style={{ color: "var(--fg-3)", fontSize: 12.5, marginLeft: 6 }}>{user}</span>
          <button className="btn btn-ghost btn-sm" title="Settings"><Icon.Settings size={14}/></button>
          <button className="btn btn-ghost btn-sm" onClick={onLogout} title="Sign out"><Icon.Logout size={14}/></button>
        </div>
      </header>

      {/* Body */}
      <div style={{ flex: 1, overflow: "auto", padding: "40px 28px 60px" }}>
        <div style={{ maxWidth: 1100, margin: "0 auto" }}>
          <div style={{
            display: "flex", alignItems: "flex-end", justifyContent: "space-between",
            marginBottom: 28, gap: 24,
          }}>
            <div>
              <div style={{ color: "var(--fg-3)", fontSize: 12, letterSpacing: "0.08em", textTransform: "uppercase", marginBottom: 8 }}>
                Workspace · ~/DirectClaude
              </div>
              <h1 style={{
                fontFamily: "var(--font-display)", fontSize: 32, fontWeight: 600,
                margin: 0, letterSpacing: "-0.02em", color: "var(--fg-0)",
              }}>
                Choose a project
              </h1>
              <div style={{ color: "var(--fg-2)", marginTop: 6, fontSize: 14 }}>
                Resume work, or start a new firmware project from a prompt.
              </div>
            </div>

            <div style={{ display: "flex", gap: 10 }}>
              <div style={{ position: "relative" }}>
                <Icon.Search size={14} style={{ position: "absolute", left: 12, top: 11, color: "var(--fg-3)" }}/>
                <input className="field" placeholder="Search projects…"
                       value={q} onChange={e => setQ(e.target.value)}
                       style={{ paddingLeft: 34, width: 260 }}/>
              </div>
              <button className="btn btn-primary" onClick={onNew}>
                <Icon.Plus size={14}/> New project
              </button>
            </div>
          </div>

          {/* Project grid */}
          <div style={{
            display: "grid", gridTemplateColumns: "repeat(auto-fill, minmax(320px, 1fr))",
            gap: 16,
          }}>
            {/* New project tile */}
            <button onClick={onNew} className="card anim-up" style={{
              padding: 22, textAlign: "left", cursor: "pointer", border: "1px dashed var(--border-2)",
              background: "transparent", display: "flex", flexDirection: "column", gap: 12,
              minHeight: 168, transition: "all .15s",
            }}
            onMouseEnter={e => { e.currentTarget.style.borderColor = "var(--border-strong)"; e.currentTarget.style.background = "rgba(139,111,245,0.04)"; }}
            onMouseLeave={e => { e.currentTarget.style.borderColor = "var(--border-2)"; e.currentTarget.style.background = "transparent"; }}>
              <div style={{
                width: 38, height: 38, borderRadius: 10, display: "flex", alignItems: "center", justifyContent: "center",
                background: "rgba(139,111,245,0.12)", color: "var(--violet-200)",
              }}>
                <Icon.Plus size={18}/>
              </div>
              <div>
                <div style={{ color: "var(--fg-0)", fontSize: 15, fontWeight: 500 }}>Start a new project</div>
                <div style={{ color: "var(--fg-3)", fontSize: 12.5, marginTop: 4 }}>Type a prompt — CCoreAi scaffolds the firmware.</div>
              </div>
            </button>

            {filtered.map((p, i) => (
              <div key={p.id} className="card anim-up" style={{
                padding: 22, textAlign: "left", cursor: "pointer",
                display: "flex", flexDirection: "column", gap: 14,
                minHeight: 168, transition: "all .15s",
                animationDelay: (i * 30) + "ms",
                background: "linear-gradient(180deg, rgba(27,22,56,0.7) 0%, rgba(20,16,42,0.7) 100%)",
                position: "relative",
              }}
              onClick={() => onOpen(p)}
              onMouseEnter={e => { e.currentTarget.style.borderColor = "var(--border-strong)"; e.currentTarget.style.transform = "translateY(-2px)"; }}
              onMouseLeave={e => { e.currentTarget.style.borderColor = "var(--border-1)"; e.currentTarget.style.transform = "translateY(0)"; }}>
                <div style={{ display: "flex", justifyContent: "space-between", alignItems: "flex-start", gap: 12 }}>
                  <div style={{ display: "flex", gap: 10, alignItems: "center" }}>
                    <div style={{
                      width: 38, height: 38, borderRadius: 10, display: "flex", alignItems: "center", justifyContent: "center",
                      background: "rgba(139,111,245,0.10)", color: "var(--violet-200)", border: "1px solid var(--border-1)",
                    }}>
                      <Icon.Cpu size={18}/>
                    </div>
                    <div>
                      <div style={{ color: "var(--fg-0)", fontSize: 15, fontWeight: 500 }}>{p.name}</div>
                      <div style={{ color: "var(--fg-3)", fontSize: 11.5, fontFamily: "var(--font-mono)", marginTop: 2 }}>{p.mcu}</div>
                    </div>
                  </div>
                  <div style={{ display: "flex", alignItems: "center", gap: 6 }}>
                    {statusPill(p.status, p.statusLabel)}
                    <button
                      className="btn btn-ghost btn-sm"
                      title="Delete project"
                      onClick={(e) => { e.stopPropagation(); if (confirm("Delete project '" + p.name + "'? This cannot be undone.")) onDelete && onDelete(p); }}
                      style={{ padding: "4px 6px", color: "var(--fg-3)" }}
                      onMouseEnter={e => { e.currentTarget.style.color = "var(--err)"; }}
                      onMouseLeave={e => { e.currentTarget.style.color = "var(--fg-3)"; }}
                    >
                      <Icon.Trash size={13}/>
                    </button>
                  </div>
                </div>

                <div style={{ color: "var(--fg-2)", fontSize: 13, lineHeight: 1.5, flex: 1 }}>
                  {p.summary}
                </div>

                <div style={{
                  display: "flex", justifyContent: "space-between", alignItems: "center",
                  fontSize: 11.5, color: "var(--fg-3)", paddingTop: 10,
                  borderTop: "1px solid var(--border-1)",
                }}>
                  <span style={{ display: "inline-flex", alignItems: "center", gap: 5 }}>
                    <Icon.Clock size={12}/> {p.lastRun || "never"}
                  </span>
                  <span style={{ display: "inline-flex", alignItems: "center", gap: 12 }}>
                    <span style={{ display: "inline-flex", alignItems: "center", gap: 4 }}><Icon.Hash size={11}/>{p.iterations}</span>
                    <span style={{ display: "inline-flex", alignItems: "center", gap: 4 }}><Icon.Folder size={11}/>{p.files}</span>
                  </span>
                </div>
              </div>
            ))}
          </div>
        </div>
      </div>
    </div>
  );
};

window.ProjectSelector = ProjectSelector;
