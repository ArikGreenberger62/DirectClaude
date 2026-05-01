// Inline SVG icons. Lucide-ish. Single stroke, currentColor.
const Ico = ({ size = 16, children, style }) => (
  <svg width={size} height={size} viewBox="0 0 24 24" fill="none"
       stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" strokeLinejoin="round"
       style={style}>{children}</svg>
);

const Icon = {
  Cpu: (p) => <Ico {...p}><rect x="4" y="4" width="16" height="16" rx="2"/><rect x="9" y="9" width="6" height="6"/><path d="M9 1v3M15 1v3M9 20v3M15 20v3M20 9h3M20 15h3M1 9h3M1 15h3"/></Ico>,
  Play: (p) => <Ico {...p}><polygon points="6 4 20 12 6 20 6 4" fill="currentColor" stroke="none"/></Ico>,
  Stop: (p) => <Ico {...p}><rect x="6" y="6" width="12" height="12" rx="1.5" fill="currentColor" stroke="none"/></Ico>,
  Pause:(p) => <Ico {...p}><rect x="7" y="5" width="3.5" height="14"/><rect x="13.5" y="5" width="3.5" height="14"/></Ico>,
  Plus: (p) => <Ico {...p}><path d="M12 5v14M5 12h14"/></Ico>,
  Folder:(p)=> <Ico {...p}><path d="M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V7z"/></Ico>,
  Search:(p)=> <Ico {...p}><circle cx="11" cy="11" r="7"/><path d="m20 20-3.5-3.5"/></Ico>,
  Send: (p) => <Ico {...p}><path d="M22 2 11 13"/><path d="m22 2-7 20-4-9-9-4 20-7z"/></Ico>,
  Check:(p) => <Ico {...p}><path d="M5 13l4 4L19 7"/></Ico>,
  X:    (p) => <Ico {...p}><path d="M18 6 6 18M6 6l12 12"/></Ico>,
  Alert:(p) => <Ico {...p}><path d="M12 9v4M12 17h.01"/><path d="M10.3 3.86 1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"/></Ico>,
  Settings:(p)=> <Ico {...p}><circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 2.83-2.83l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9c.36.16.66.42.86.74.2.32.31.69.31 1.07v.38c0 .38-.11.75-.31 1.07-.2.32-.5.58-.86.74z"/></Ico>,
  Logout:(p) => <Ico {...p}><path d="M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4"/><path d="m16 17 5-5-5-5"/><path d="M21 12H9"/></Ico>,
  ChevronRight:(p)=> <Ico {...p}><path d="m9 6 6 6-6 6"/></Ico>,
  ChevronDown:(p)=> <Ico {...p}><path d="m6 9 6 6 6-6"/></Ico>,
  Lock: (p) => <Ico {...p}><rect x="4" y="11" width="16" height="10" rx="2"/><path d="M8 11V7a4 4 0 0 1 8 0v4"/></Ico>,
  User: (p) => <Ico {...p}><circle cx="12" cy="8" r="4"/><path d="M4 21a8 8 0 0 1 16 0"/></Ico>,
  Eye:  (p) => <Ico {...p}><path d="M2 12s3.5-7 10-7 10 7 10 7-3.5 7-10 7S2 12 2 12z"/><circle cx="12" cy="12" r="3"/></Ico>,
  EyeOff:(p)=> <Ico {...p}><path d="M3 3l18 18M10.6 6.1A10.1 10.1 0 0 1 12 6c6.5 0 10 6 10 6a17.7 17.7 0 0 1-3.3 4.2M6.6 6.6A17.4 17.4 0 0 0 2 12s3.5 6 10 6c1.6 0 3-.3 4.2-.8M9.9 9.9a3 3 0 0 0 4.2 4.2"/></Ico>,
  Copy: (p) => <Ico {...p}><rect x="9" y="9" width="13" height="13" rx="2"/><path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"/></Ico>,
  Sparkle:(p)=> <Ico {...p}><path d="M12 3v4M12 17v4M3 12h4M17 12h4M5.6 5.6l2.8 2.8M15.6 15.6l2.8 2.8M5.6 18.4l2.8-2.8M15.6 8.4l2.8-2.8"/></Ico>,
  Zap:  (p) => <Ico {...p}><path d="M13 2 3 14h8l-1 8 10-12h-8l1-8z"/></Ico>,
  Code: (p) => <Ico {...p}><polyline points="16 18 22 12 16 6"/><polyline points="8 6 2 12 8 18"/></Ico>,
  Clock:(p) => <Ico {...p}><circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/></Ico>,
  Hash: (p) => <Ico {...p}><path d="M4 9h16M4 15h16M10 3 8 21M16 3l-2 18"/></Ico>,
  Terminal:(p)=> <Ico {...p}><polyline points="4 17 10 11 4 5"/><line x1="12" y1="19" x2="20" y2="19"/></Ico>,
  Cable:(p) => <Ico {...p}><path d="M4 4v3a2 2 0 0 0 2 2h0a2 2 0 0 1 2 2v6a2 2 0 0 0 2 2h0"/><path d="M14 21v-3a2 2 0 0 1 2-2h0a2 2 0 0 0 2-2V8a2 2 0 0 1 2-2h0"/><path d="M2 4h4M18 20h4M9 2h2M13 22h2"/></Ico>,
  Bolt: (p) => <Ico {...p}><path d="M13 2 3 14h8l-1 8 10-12h-8l1-8z" fill="currentColor" stroke="none"/></Ico>,
  Wrench:(p)=> <Ico {...p}><path d="M14.7 6.3a4 4 0 1 0 5 5L21 14l-4 4-2.3-2.3-7.4 7.4a2.1 2.1 0 0 1-3-3L11.7 13 14.7 6.3z"/></Ico>,
  CircleDot:(p)=> <Ico {...p}><circle cx="12" cy="12" r="9"/><circle cx="12" cy="12" r="3" fill="currentColor"/></Ico>,
  Pin:  (p) => <Ico {...p}><path d="M12 17v5"/><path d="M9 10.76V6a1 1 0 0 1 1-1h4a1 1 0 0 1 1 1v4.76a2 2 0 0 0 .55 1.39L18 15H6l2.45-2.85A2 2 0 0 0 9 10.76z"/></Ico>,
  Trash:(p) => <Ico {...p}><path d="M3 6h18M8 6V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2M19 6l-1 14a2 2 0 0 1-2 2H8a2 2 0 0 1-2-2L5 6"/></Ico>,
  Spark:(p) => <Ico {...p}><path d="M12 2 14 9l7 1-5.5 4.5L17 22l-5-4-5 4 1.5-7.5L3 10l7-1z" fill="currentColor" stroke="none"/></Ico>,
};

window.Icon = Icon;
