// SwiftTerm behind a C interface (include/t5m_term.h) for the firmware's C++.
import SwiftTerm

final class TermHost: TerminalDelegate {
  var terminal: Terminal!
  var outbound: [UInt8] = []
  var title: [UInt8] = []
  var titleGeneration: UInt32 = 0
  var bells: UInt32 = 0
  var cells: [t5m_cell] = []

  func send(source: Terminal, data: ArraySlice<UInt8>) { outbound.append(contentsOf: data) }
  func showCursor(source: Terminal) {}
  func hideCursor(source: Terminal) {}
  func setTerminalTitle(source: Terminal, title: String) {
    self.title = Array(title.utf8)
    titleGeneration &+= 1
  }
  func bell(source: Terminal) { bells &+= 1 }
}

@inline(__always) func host(_ p: UnsafeMutableRawPointer) -> TermHost {
  Unmanaged<TermHost>.fromOpaque(p).takeUnretainedValue()
}

func rgb(_ c: Color) -> UInt32 {
  func byte(_ v: UInt16) -> UInt32 { (UInt32(v) * 255 + 32767) / 65535 }
  return byte(c.red) << 16 | byte(c.green) << 8 | byte(c.blue)
}

func resolve(_ value: Attribute.Color, _ s: TerminalRenderSnapshot, foreground: Bool) -> UInt32 {
  let useForeground = foreground != s.reverseVideo
  switch value {
  case .defaultColor: return rgb(useForeground ? s.foregroundColor : s.backgroundColor)
  case .defaultInvertedColor: return rgb(useForeground ? s.backgroundColor : s.foregroundColor)
  case .ansi256(let code):
    return Int(code) < s.palette.count ? rgb(s.palette[Int(code)]) : rgb(s.foregroundColor)
  case .trueColor(let r, let g, let b): return UInt32(r) << 16 | UInt32(g) << 8 | UInt32(b)
  }
}

@_cdecl("t5m_term_new")
public func t5mTermNew(_ cols: Int32, _ rows: Int32, _ scrollback: Int32) -> UnsafeMutableRawPointer {
  let h = TermHost()
  h.terminal = Terminal(delegate: h, options: TerminalOptions(cols: Int(cols), rows: Int(rows), scrollback: Int(scrollback)))
  // Light text on the Multitool's background (SwiftTerm's default text is a dim grey).
  h.terminal.foregroundColor = Color(red8: 0xDC, green8: 0xE1, blue8: 0xE6)
  h.terminal.backgroundColor = Color(red8: 0x10, green8: 0x14, blue8: 0x18)
  return Unmanaged.passRetained(h).toOpaque()
}

@_cdecl("t5m_term_free")
public func t5mTermFree(_ p: UnsafeMutableRawPointer) {
  let h = Unmanaged<TermHost>.fromOpaque(p)
  h.takeUnretainedValue().terminal.close()  // breaks the terminal <-> delegate cycle
  h.release()
}

@_cdecl("t5m_term_resize")
public func t5mTermResize(_ p: UnsafeMutableRawPointer, _ cols: Int32, _ rows: Int32) {
  host(p).terminal.resize(cols: Int(cols), rows: Int(rows))
}

@_cdecl("t5m_term_feed")
public func t5mTermFeed(_ p: UnsafeMutableRawPointer, _ data: UnsafePointer<UInt8>, _ n: Int32) {
  host(p).terminal.feed(buffer: ArraySlice(UnsafeBufferPointer(start: data, count: Int(n))))
}

@_cdecl("t5m_term_text")
public func t5mTermText(_ p: UnsafeMutableRawPointer, _ utf8: UnsafePointer<UInt8>, _ n: Int32) -> Int32 {
  let text = String(decoding: UnsafeBufferPointer(start: utf8, count: Int(n)), as: UTF8.self)
  return host(p).terminal.sendHostText(text) ? 1 : 0
}

@_cdecl("t5m_term_key")
public func t5mTermKey(_ p: UnsafeMutableRawPointer, _ key: UnsafePointer<CChar>, _ code: UnsafePointer<CChar>,
                       _ mods: UInt32) -> Int32 {
  let t = host(p).terminal!
  // 1 = press (Kitty event type); 1 back means bytes were queued.
  return t.sendHostKey(key: String(cString: key), code: String(cString: code), modifiers: mods,
                       eventType: 1, text: nil) == 1 ? 1 : 0
}

@_cdecl("t5m_term_output")
public func t5mTermOutput(_ p: UnsafeMutableRawPointer, _ buf: UnsafeMutablePointer<UInt8>, _ cap: Int32) -> Int32 {
  let h = host(p)
  let n = min(Int(cap), h.outbound.count)
  for i in 0..<n { buf[i] = h.outbound[i] }
  h.outbound.removeFirst(n)
  return Int32(n)
}

@_cdecl("t5m_term_frame")
public func t5mTermFrame(_ p: UnsafeMutableRawPointer, _ full: Int32, _ fn: t5m_row_fn,
                         _ ctx: UnsafeMutableRawPointer?, _ cursor: UnsafeMutablePointer<t5m_cursor>,
                         _ bg: UnsafeMutablePointer<UInt32>) -> Int32 {
  let h = host(p)
  let s = h.terminal.makeRenderSnapshot(scope: full != 0 ? .full : .dirty)
  var defaultBg = rgb(s.backgroundColor)
  if s.reverseVideo { defaultBg = rgb(s.foregroundColor) }
  bg.pointee = defaultBg
  let shape: UInt8
  switch s.cursor.style {
  case .blinkBlock, .steadyBlock: shape = 0
  case .blinkUnderline, .steadyUnderline: shape = 1
  case .blinkBar, .steadyBar: shape = 2
  }
  cursor.pointee = t5m_cursor(x: Int32(s.cursor.x), y: Int32(s.cursor.y), visible: s.cursor.hidden ? 0 : 1,
                              shape: shape, blink: s.cursor.blink ? 1 : 0, pad: 0,
                              color: s.cursorColor.map { rgb($0) } ?? rgb(s.foregroundColor))
  var drawn: Int32 = 0
  for row in s.lines {
    h.cells.removeAll(keepingCapacity: true)
    for cell in row.cells {
      let a = cell.attribute
      var fg = resolve(a.fg, s, foreground: true)
      var cbg = resolve(a.bg, s, foreground: false)
      if a.style.contains(.inverse) { swap(&fg, &cbg) }
      var width = UInt8(clamping: cell.width)
      if cell.widthState == .spacerTail { width = 0 }
      h.cells.append(t5m_cell(ch: cell.text.unicodeScalars.first?.value ?? 0, fg: fg, bg: cbg,
                              style: a.style.rawValue, width: width, pad: (0, 0)))
    }
    h.cells.withUnsafeBufferPointer { fn(ctx, Int32(row.y), $0.baseAddress, Int32($0.count)) }
    drawn += 1
  }
  h.terminal.clearUpdateRange()
  return drawn
}

@_cdecl("t5m_term_scroll")
public func t5mTermScroll(_ p: UnsafeMutableRawPointer, _ lines: Int32) -> Int32 {
  let t = host(p).terminal!
  t.scrollViewport(Int(lines))
  let v = t.viewportState()
  return Int32(v.maximumTopRow - v.topRow)
}

@_cdecl("t5m_term_scroll_live")
public func t5mTermScrollLive(_ p: UnsafeMutableRawPointer) {
  let t = host(p).terminal!
  t.scrollViewport(t.viewportState().maximumTopRow, absolute: true)
}

@_cdecl("t5m_term_title")
public func t5mTermTitle(_ p: UnsafeMutableRawPointer, _ buf: UnsafeMutablePointer<CChar>, _ cap: Int32) -> UInt32 {
  let h = host(p)
  let n = min(Int(cap) - 1, h.title.count)
  for i in 0..<n { buf[i] = CChar(bitPattern: h.title[i]) }
  buf[max(n, 0)] = 0
  return h.titleGeneration
}

@_cdecl("t5m_term_bells")
public func t5mTermBells(_ p: UnsafeMutableRawPointer) -> UInt32 { host(p).bells }
