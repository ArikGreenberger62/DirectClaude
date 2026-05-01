// Login screen — bridge-aware version.
// Asks for the local bridge token (printed in the terminal at startup) and
// validates it against /api/auth before advancing.
const Login = ({ onLogin }) => {
  const [user, setUser] = React.useState("arik");
  const [token, setToken] = React.useState(window.CCB ? CCB.getToken() : "");
  const [showTok, setShowTok] = React.useState(false);
  const [loading, setLoading] = React.useState(false);
  const [err, setErr] = React.useState("");
  const [healthInfo, setHealthInfo] = React.useState(null);

  React.useEffect(() => {
    if (!window.CCB) return;
    CCB.health().then(setHealthInfo).catch(() => setHealthInfo(null));
    if (CCB.hasToken()) {
      CCB.listProjects().then(() => onLogin(user)).catch(() => {});
    }
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  const submit = async (e) => {
    e.preventDefault();
    setErr("");
    setLoading(true);
    try {
      if (!window.CCB) throw new Error("CCB runtime missing");
      await CCB.login(token.trim());
      onLogin(user);
    } catch (ex) {
      setErr(ex.message || "Login failed");
    } finally {
      setLoading(false);
    }
  };

  return (
    <div style={{
      flex: 1, display: "flex", alignItems: "center", justifyContent: "center",
      padding: 32, position: "relative",
    }}>
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

      <div className="card anim-up" style={{ width: 420, padding: 36, position: "relative" }}>
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
                     value={(healthInfo && healthInfo.workspace) || "~/DirectClaude"} readOnly/>
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
            <label className="label">Bridge token</label>
            <div style={{ position: "relative" }}>
              <Icon.Lock size={15} style={{ position: "absolute", left: 12, top: 12, color: "var(--fg-3)" }}/>
              <input className="field" type={showTok ? "text" : "password"}
                     style={{ paddingLeft: 36, paddingRight: 38 }}
                     placeholder="paste the token printed in the bridge terminal"
                     value={token} onChange={e => setToken(e.target.value)}/>
              <button type="button" onClick={() => setShowTok(s => !s)}
                      style={{
                        position: "absolute", right: 8, top: 8,
                        background: "transparent", border: 0, color: "var(--fg-3)",
                        padding: 4, cursor: "pointer", display: "flex",
                      }}>
                {showTok ? <Icon.EyeOff size={15}/> : <Icon.Eye size={15}/>}
              </button>
            </div>
            <div style={{ color: "var(--fg-3)", fontSize: 11.5, marginTop: 6 }}>
              Printed by <span style={{ fontFamily: "var(--font-mono)" }}>ccorebridge</span> on startup.
              Stored in <span style={{ fontFamily: "var(--font-mono)" }}>~/.ccorebridge/token</span>.
            </div>
          </div>

          {err && (
            <div style={{
              marginBottom: 14, padding: "8px 12px", borderRadius: 8,
              background: "rgba(248,113,113,0.10)", border: "1px solid rgba(248,113,113,0.35)",
              color: "#FCA5A5", fontSize: 12,
            }}>{err}</div>
          )}

          <button className="btn btn-primary" style={{ width: "100%", padding: "11px 14px" }} type="submit" disabled={loading || !token.trim()}>
            {loading ? <><span className="pulse">Connecting…</span></> : <><Icon.Bolt size={14}/> Sign in to Mission Control</>}
          </button>

          <div style={{
            marginTop: 18, paddingTop: 16, borderTop: "1px solid var(--border-1)",
            display: "flex", justifyContent: "space-between", alignItems: "center",
            fontSize: 11.5, color: "var(--fg-3)",
          }}>
            <span style={{ display: "flex", alignItems: "center", gap: 6 }}>
              <span className={"dot " + (healthInfo ? "ok" : "warn")} style={{ width: 6, height: 6, boxShadow: "none" }}/>
              CCoreBridge {healthInfo ? "online · v" + healthInfo.version : "offline"}
            </span>
            <span>Local · {location.host}</span>
          </div>
        </form>
      </div>
    </div>
  );
};

window.Login = Login;
