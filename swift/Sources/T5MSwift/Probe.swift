// A self-test for the Embedded Swift build on the Tab5: classes (heap +
// refcounting), arrays, dictionaries, String and Unicode, and floats. The
// firmware's console command 'w' calls it.

final class Box {
  var values: [Int] = []
  init(_ n: Int) { for i in 0..<n { values.append(i * i) } }
}

@_cdecl("t5m_swift_probe")
public func t5mSwiftProbe(_ out: UnsafeMutablePointer<UInt8>, _ cap: Int) -> Int {
  let box = Box(10)
  var words: [String: Int] = [:]
  for w in ["tab5", "swift", "term", "swift"] { words[w, default: 0] += 1 }
  let text = "héllo → 世界 \u{1F600}"
  let half: Float = Float(box.values.reduce(0, +)) / 2
  let line = "sum=\(box.values.reduce(0, +)) swift=\(words["swift"] ?? 0) chars=\(text.count) utf8=\(text.utf8.count) half=\(Int(half * 10))"
  var n = 0
  for b in line.utf8 where n < cap - 1 { out[n] = b; n += 1 }
  out[n] = 0
  return n
}
