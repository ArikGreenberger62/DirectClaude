// Login screen
const Login = ({ onLogin }) => {
  const [user, setUser] = React.useState("arik");
  const [pw, setPw] = React.useState("••••••••");
  const [showPw, setShowPw] = React.useState(false);
  const [loading, setLoading] = React.useState(false);

  const submit = (e) => {
    e.preventDefault();
    setLoading(true);
    setTimeout(() => { setLoading(false); onLogin(user); }, 600);
  };

  return (
    <div style={{
      flex: 1, display: "flex", alignItems: "center", justifyContent: "center",
      padding: 32, position: "relative",
    }}>
      {/* Orbital ring decoration */}
      <div style={{
        position: "absolute", left: "50%", top: "50%",
        width: 720, height: 720, marginLeft: -360, marginTop: -360,
        border: "1px solid rgba(139,111,245,0.08)", borderRadius: "50%",
        pointerEvents: "none",
      }}/>
      <div style={{
        position: "absolute", left: "50%", top: "50%",
        width: 980, height: 980, marginLeft: -490, marginTop: -490,
        border: "1px dashed rgba(228,91,216,0.06)", borderRadius: "50%",
        pointerEvents: "none",
      }}/>

      <div className="card anim-up" style={{
        width: 420, padding: 36, position: "relative",
      }}>
        <div style={{ display: "flex", flexDirection: "column", alignItems: "center", marginBottom: 26 }}>
          <div className="logo-mark lg glow" style={{ marginBottom: 14 }}/>
          <div className="logo-name" style={{
            fontFamily: "var(--font-display)", fontSize: 24, fontWeight: 600, letterSpacing: "-0.02em",
          }}>
            CCore<span className="accent">Ai</span>
          </div>
          <div style={{ color: "var(--fg-2)", fontSize: 12.5, marginTop: 4, letterSpacing: "0.06em", textTransform: "uppercase" }}>
            AI-Driven Embedded Synthesis
          </div>
        </div>

        <form onSubmit={submit}>
          <div style={{ marginBottom: 16 }}>
            <label className="label">Workspace</label>
            <div style={{ position: "relative" }}>
              <Icon.Folder size={15} style={{ position: "absolute", left: 12, top: 12, color: "var(--fg-3)" }}/>
              <input className="field" style={{ paddingLeft: 36 }}
                     value="~/DirectClaude" readOnly/>
            </div>
          </div>

          <div style={{ marginBottom: 16 }}>
            <label className="label">User</label>
            <div style={{ position: "relative" }}>
              <Icon.User size={15} style={{ position: "absolute", left: 12, top: 12, color: "var(--fg-3)" }}/>
              <input className="field" style={{ paddingLeft: 36 }}
                     value={user} onChange={e => setUser(e.target.value)} />
            </div>
          </div>

          <div style={{ marginBottom: 22 }}>
            <label className="label">API key</label>
            <div style={{ position: "relative" }}>
              <Icon.Lock size={15} style={{ position: "absolute", left: 12, top: 12, color: "var(--fg-3)" }}/>
              <input className="field" type={showPw ? "text" : "password"}
                     style={{ paddingLeft: 36, paddingRight: 38 }}
                     value={pw} onChange={e => setPw(e.target.value)}/>
              <button type="button" onClick={() => setShowPw(s => !s)}
                      style={{
                        position: "absolute", right: 8, top: 8,
                        background: "transparent", border: 0, color: "var(--fg-3)",
                        padding: 4, cursor: "pointer", display: "flex",
                      }}>
                {showPw ? <Icon.EyeOff size={15}/> : <Icon.Eye size={15}/>}
              </button>
            </div>
            <div style={{ color: "var(--fg-3)", fontSize: 11.5, marginTop: 6 }}>
              Loaded from <span style={{ fontFamily: "var(--font-mono)" }}>~/DirectClaude/ApiKeyArik</span>
            </div>
          </div>

          <button className="btn btn-primary" style={{ width: "100%", padding: "11px 14px" }} type="submit" disabled={loading}>
            {loading ? <><span className="pulse">Connecting…</span></> : <><Icon.Bolt size={14}/> Sign in to Mission Control</>}
          </button>

          <div style={{
            marginTop: 18, paddingTop: 16, borderTop: "1px solid var(--border-1)",
            display: "flex", justifyContent: "space-between", alignItems: "center",
            fontSize: 11.5, color: "var(--fg-3)",
          }}>
            <span style={{ display: "flex", alignItems: "center", gap: 6 }}>
              <span className="dot ok" style={{ width: 6, height: 6, boxShadow: "none" }}/>
              CCoreBridge online · v0.3.1
            </span>
            <span>Local · 127.0.0.1:8765</span>
          </div>
        </form>
      </div>
    </div>
  );
};

window.Login = Login;
