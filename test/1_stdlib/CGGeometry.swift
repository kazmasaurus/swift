// RUN: %target-run-simple-swift | FileCheck %s
// REQUIRES: executable_test

// REQUIRES: objc_interop

import CoreGraphics

func print_(r: CGPoint, _ prefix: String) {
  print("\(prefix) \(r.x) \(r.y)")
}
func print_(r: CGSize, _ prefix: String) {
  print("\(prefix) \(r.width) \(r.height)")
}

func print_(r: CGVector, _ prefix: String) {
  print("\(prefix) \(r.dx) \(r.dy)")
}

func print_(r: CGRect, _ prefix: String) {
  print("\(prefix) \(r.origin.x) \(r.origin.y) \(r.size.width) \(r.size.height)")
}

func print_(r: CGAffineTransform, _ prefix: String) {
  print("\(prefix) \(r.a) \(r.b) \(r.c) \(r.d) \(r.tx) \(r.ty)")
}

let int1: Int = 1
let int2: Int = 2
let int3: Int = 3
let int4: Int = 4
let int5: Int = 5
let int6: Int = 6

let cgfloat1: CGFloat = 1
let cgfloat2: CGFloat = 2
let cgfloat3: CGFloat = 3
let cgfloat4: CGFloat = 4
let cgfloat5: CGFloat = 5
let cgfloat6: CGFloat = 6

let double1: Double = 1
let double2: Double = 2
let double3: Double = 3
let double4: Double = 4
let double5: Double = 5
let double6: Double = 6

print("You may begin.")
// CHECK: You may begin.


var pt: CGPoint

pt = CGPoint(x: 1.25, y: 2.25)
print_(pt, "named float literals")
pt = CGPoint(x: 1, y: 2)
print_(pt, "named int literals")
pt = CGPoint(x: cgfloat1, y: cgfloat2)
print_(pt, "named cgfloats")
pt = CGPoint(x: double1, y: double2)
print_(pt, "named doubles")
pt = CGPoint(x: int1, y: int2)
print_(pt, "named ints")
// CHECK-NEXT: named float literals 1.25 2.25
// CHECK-NEXT: named int literals 1.0 2.0
// CHECK-NEXT: named cgfloats 1.0 2.0
// CHECK-NEXT: named doubles 1.0 2.0
// CHECK-NEXT: named ints 1.0 2.0

assert(pt != CGPoint.zero)


var size: CGSize

size = CGSize(width:-1.25, height:-2.25)
print_(size, "named float literals")
size = CGSize(width:-1, height:-2)
print_(size, "named int literals")
size = CGSize(width:cgfloat1, height:cgfloat2)
print_(size, "named cgfloats")
size = CGSize(width:double1, height:double2)
print_(size, "named doubles")
size = CGSize(width: int1, height: int2)
print_(size, "named ints")
// CHECK-NEXT: named float literals -1.25 -2.25
// CHECK-NEXT: named int literals -1.0 -2.0
// CHECK-NEXT: named cgfloats 1.0 2.0
// CHECK-NEXT: named doubles 1.0 2.0
// CHECK-NEXT: named ints 1.0 2.0

assert(size != CGSize.zero)


var vector: CGVector

vector = CGVector(dx: -111.25, dy: -222.25)
print_(vector, "named float literals")
vector = CGVector(dx: -111, dy: -222)
print_(vector, "named int literals")
vector = CGVector(dx: cgfloat1, dy: cgfloat2)
print_(vector, "named cgfloats")
vector = CGVector(dx: double1, dy: double2)
print_(vector, "named doubles")
vector = CGVector(dx: int1, dy: int2)
print_(vector, "named ints")
// CHECK-NEXT: named float literals -111.25 -222.25
// CHECK-NEXT: named int literals -111.0 -222.0
// CHECK-NEXT: named cgfloats 1.0 2.0
// CHECK-NEXT: named doubles 1.0 2.0
// CHECK-NEXT: named ints 1.0 2.0

assert(vector != CGVector.zero)


var rect: CGRect

pt = CGPoint(x: 10.25, y: 20.25)
size = CGSize(width: 30.25, height: 40.25)
rect = CGRect(origin: pt, size: size)
print_(rect, "point+size")
rect = CGRect(origin:pt, size:size)
print_(rect, "named point+size")
// CHECK-NEXT: point+size 10.25 20.25 30.25 40.25
// CHECK-NEXT: named point+size 10.25 20.25 30.25 40.25

rect = CGRect(x:10.25, y:20.25, width:30.25, height:40.25)
print_(rect, "named float literals")
rect = CGRect(x:10, y:20, width:30, height:40)
print_(rect, "named int literals")
rect = CGRect(x:cgfloat1, y:cgfloat2, width:cgfloat3, height:cgfloat4)
print_(rect, "named cgfloats")
rect = CGRect(x:double1, y:double2, width:double3, height:double4)
print_(rect, "named doubles")
rect = CGRect(x:int1, y:int2, width:int3, height:int4)
print_(rect, "named ints")
// CHECK-NEXT: named float literals 10.25 20.25 30.25 40.25
// CHECK-NEXT: named int literals 10.0 20.0 30.0 40.0
// CHECK-NEXT: named cgfloats 1.0 2.0 3.0 4.0
// CHECK-NEXT: named doubles 1.0 2.0 3.0 4.0
// CHECK-NEXT: named ints 1.0 2.0 3.0 4.0

assert(rect == rect)
assert(rect != CGRect.zero)
assert(!rect.isNull)
assert(!rect.isEmpty)
assert(!rect.isInfinite)
assert(CGRect.null.isNull)
assert(CGRect.zero.isEmpty)
assert(CGRect.infinite.isInfinite)


var unstandard = CGRect(x: 10, y: 20, width: -30, height: -50)
var standard = unstandard.standardized
print_(unstandard, "unstandard")
print_(standard, "standard")
// CHECK-NEXT: unstandard 10.0 20.0 -30.0 -50.0
// CHECK-NEXT: standard -20.0 -30.0 30.0 50.0

assert(unstandard.width == 30)
assert(unstandard.size.width == -30)
assert(standard.width == 30)
assert(standard.size.width == 30)

assert(unstandard.height == 50)
assert(unstandard.size.height == -50)
assert(standard.height == 50)
assert(standard.size.height == 50)

assert(unstandard.minX == -20)
assert(unstandard.midX == -5)
assert(unstandard.maxX == 10)

assert(unstandard.minY == -30)
assert(unstandard.midY == -5)
assert(unstandard.maxY == 20)

assert(unstandard == standard)
assert(unstandard.standardized == standard)

unstandard.standardizeInPlace()
print_(unstandard, "standardized unstandard")
// CHECK-NEXT: standardized unstandard -20.0 -30.0 30.0 50.0

rect = CGRect(x: 11.25, y: 22.25, width: 33.25, height: 44.25)
print_(rect.insetBy(dx: 1, dy: -2), "insetBy")
// CHECK-NEXT: insetBy 12.25 20.25 31.25 48.25
rect.insetInPlace(dx: 1, dy: -2)
print_(rect, "insetInPlace")
// CHECK-NEXT: insetInPlace 12.25 20.25 31.25 48.25

rect = CGRect(x: 11.25, y: 22.25, width: 33.25, height: 44.25)
print_(rect.offsetBy(dx: 3, dy: -4), "offsetBy")
// CHECK-NEXT: offsetBy 14.25 18.25 33.25 44.25
rect.offsetInPlace(dx: 3, dy: -4)
print_(rect, "offsetInPlace")
// CHECK-NEXT: offsetInPlace 14.25 18.25 33.25 44.25

rect = CGRect(x: 11.25, y: 22.25, width: 33.25, height: 44.25)
print_(rect.integral, "integral")
// CHECK-NEXT: integral 11.0 22.0 34.0 45.0
rect.makeIntegralInPlace()
print_(rect, "makeIntegralInPlace")
// CHECK-NEXT: makeIntegralInPlace 11.0 22.0 34.0 45.0

let smallRect = CGRect(x: 10, y: 25, width: 5, height: -5)
let bigRect = CGRect(x: 1, y: 2, width: 101, height: 102)
let distantRect = CGRect(x: 1000, y: 2000, width: 1, height: 1)

rect = CGRect(x: 11.25, y: 22.25, width: 33.25, height: 44.25)
print_(rect.union(smallRect), "union small")
print_(rect.union(bigRect), "union big")
print_(rect.union(distantRect), "union distant")
// CHECK-NEXT: union small 10.0 20.0 34.5 46.5
// CHECK-NEXT: union big 1.0 2.0 101.0 102.0
// CHECK-NEXT: union distant 11.25 22.25 989.75 1978.75
rect.unionInPlace(smallRect)
rect.unionInPlace(bigRect)
rect.unionInPlace(distantRect)
print_(rect, "unionInPlace")
// CHECK-NEXT: unionInPlace 1.0 2.0 1000.0 1999.0

rect = CGRect(x: 11.25, y: 22.25, width: 33.25, height: 44.25)
print_(rect.intersect(smallRect), "intersect small")
print_(rect.intersect(bigRect), "intersect big")
print_(rect.intersect(distantRect), "intersect distant")
// CHECK-NEXT: intersect small 11.25 22.25 3.75 2.75
// CHECK-NEXT: intersect big 11.25 22.25 33.25 44.25
// CHECK-NEXT: intersect distant inf inf 0.0 0.0
assert(rect.intersects(smallRect))
rect.intersectInPlace(smallRect)
assert(!rect.isEmpty)
assert(rect.intersects(bigRect))
rect.intersectInPlace(bigRect)
assert(!rect.isEmpty)
assert(!rect.intersects(distantRect))
rect.intersectInPlace(distantRect)
assert(rect.isEmpty)


rect = CGRect(x: 11.25, y: 22.25, width: 33.25, height: 44.25)
assert(rect.contains(CGPoint(x: 15, y: 25)))
assert(!rect.contains(CGPoint(x: -15, y: 25)))
assert(bigRect.contains(rect))
assert(!rect.contains(bigRect))


rect = CGRect(x: 11.25, y: 22.25, width: 33.25, height: 44.25)
var (slice, remainder) = rect.divide(5, fromEdge:CGRectEdge.MinXEdge)
print_(slice, "slice")
print_(remainder, "remainder")
// CHECK-NEXT: slice 11.25 22.25 5.0 44.25
// CHECK-NEXT: remainder 16.25 22.25 28.25 44.25


var transform: CGAffineTransform

transform = CGAffineTransform(a: 1.25, b: 2.25, c: 3.25, d: 4.25, tx: 5.25, ty: 6.25)
print_(transform, "named float literals")
transform = CGAffineTransform(a: 1, b: 2, c: 3, d: 4, tx: 5, ty: 6)
print_(transform, "named int literals")
transform = CGAffineTransform(a: cgfloat1, b: cgfloat2, c: cgfloat3, d: cgfloat4, tx: cgfloat5, ty: cgfloat6)
print_(transform, "named cgfloats")
transform = CGAffineTransform(a: double1, b: double2, c: double3, d: double4, tx: double5, ty: double6)
print_(transform, "named doubles")
transform = CGAffineTransform(a: int1, b: int2, c: int3, d: int4, tx: int5, ty: int6)
print_(transform, "named ints")
// CHECK-NEXT: named float literals 1.25 2.25 3.25 4.25 5.25 6.25
// CHECK-NEXT: named int literals 1.0 2.0 3.0 4.0 5.0 6.0
// CHECK-NEXT: named cgfloats 1.0 2.0 3.0 4.0 5.0 6.0
// CHECK-NEXT: named doubles 1.0 2.0 3.0 4.0 5.0 6.0
// CHECK-NEXT: named ints 1.0 2.0 3.0 4.0 5.0 6.0

transform = CGAffineTransform(scalex: 1.25, sy: 2.25)
print_(transform, "named float literals")
transform = CGAffineTransform(scalex: 1, sy: 2)
print_(transform, "named int literals")
transform = CGAffineTransform(scalex: cgfloat1, sy: cgfloat2)
print_(transform, "named cgfloats")
transform = CGAffineTransform(scalex: double1, sy: double2)
print_(transform, "named doubles")
transform = CGAffineTransform(scalex: int1, sy: int2)
print_(transform, "named ints")
// CHECK-NEXT: named float literals 1.25 0.0 0.0 2.25 0.0 0.0
// CHECK-NEXT: named int literals 1.0 0.0 0.0 2.0 0.0 0.0
// CHECK-NEXT: named cgfloats 1.0 0.0 0.0 2.0 0.0 0.0
// CHECK-NEXT: named doubles 1.0 0.0 0.0 2.0 0.0 0.0
// CHECK-NEXT: named ints 1.0 0.0 0.0 2.0 0.0 0.0

transform = CGAffineTransform(translationx: 1.25, ty: 2.25)
print_(transform, "named float literals")
transform = CGAffineTransform(translationx: 1, ty: 2)
print_(transform, "named int literals")
transform = CGAffineTransform(translationx: cgfloat1, ty: cgfloat2)
print_(transform, "named cgfloats")
transform = CGAffineTransform(translationx: double1, ty: double2)
print_(transform, "named doubles")
transform = CGAffineTransform(translationx: int1, ty: int2)
print_(transform, "named ints")
// CHECK-NEXT: named float literals 1.0 0.0 0.0 1.0 1.25 2.25
// CHECK-NEXT: named int literals 1.0 0.0 0.0 1.0 1.0 2.0
// CHECK-NEXT: named cgfloats 1.0 0.0 0.0 1.0 1.0 2.0
// CHECK-NEXT: named doubles 1.0 0.0 0.0 1.0 1.0 2.0
// CHECK-NEXT: named ints 1.0 0.0 0.0 1.0 1.0 2.0

transform = CGAffineTransform(rotation: 1.25)
print_(transform, "named float literal")
transform = CGAffineTransform(rotation: 1)
print_(transform, "named int literal")
transform = CGAffineTransform(rotation: cgfloat1)
print_(transform, "named cgfloat")
transform = CGAffineTransform(rotation: double1)
print_(transform, "named double")
transform = CGAffineTransform(rotation: int1)
print_(transform, "named int")
// CHECK-NEXT: named float literal 0.315322362395269 0.948984619355586 -0.948984619355586 0.315322362395269 0.0 0.0
// CHECK-NEXT: named int literal 0.54030230586814 0.841470984807897 -0.841470984807897 0.54030230586814 0.0 0.0
// CHECK-NEXT: named cgfloat 0.54030230586814 0.841470984807897 -0.841470984807897 0.54030230586814 0.0 0.0
// CHECK-NEXT: named double 0.54030230586814 0.841470984807897 -0.841470984807897 0.54030230586814 0.0 0.0
// CHECK-NEXT: named int 0.54030230586814 0.841470984807897 -0.841470984807897 0.54030230586814 0.0 0.0

assert(transform == transform)
assert(transform != CGAffineTransform.identity)
assert(!transform.isIdentity)
assert(CGAffineTransform.identity.isIdentity)

transform = CGAffineTransform(a: 10.25, b: 11.25, c: 12.25, d: 13.25, tx: 14.25, ty: 15.25)
print_(transform.inverted, "inverted")
// CHECK-NEXT: inverted -6.625 5.625 6.125 -5.125 1.0 -2.0
transform.invertInPlace()
print_(transform, "invertInPlace")
// CHECK-NEXT: invertInPlace -6.625 5.625 6.125 -5.125 1.0 -2.0

transform = CGAffineTransform(a: 10.25, b: 11.25, c: 12.25, d: 13.25, tx: 14.25, ty: 15.25)
print_(transform.scaledBy(sx: 2, sy: -3), "scaledBy")
// CHECK-NEXT: scaledBy 20.5 22.5 -36.75 -39.75 14.25 15.25
transform.scaleInPlace(sx: 2, sy: -3)
print_(transform, "scaleInPlace")
// CHECK-NEXT: scaleInPlace 20.5 22.5 -36.75 -39.75 14.25 15.25

transform = CGAffineTransform(a: 10.25, b: 11.25, c: 12.25, d: 13.25, tx: 14.25, ty: 15.25)
print_(transform.translatedBy(tx: 2, ty: -3), "translatedBy")
// CHECK-NEXT: translatedBy 10.25 11.25 12.25 13.25 -2.0 -2.0
transform.translateInPlace(tx: 2, ty: -3)
print_(transform, "translateInPlace")
// CHECK-NEXT: translateInPlace 10.25 11.25 12.25 13.25 -2.0 -2.0

transform = CGAffineTransform(a: 10.25, b: 11.25, c: 12.25, d: 13.25, tx: 14.25, ty: 15.25)
print_(transform.rotatedBy(1.25), "rotatedBy")
// CHECK-NEXT: rotatedBy 14.8571158016574 16.1214227834083 -5.86439340905272 -6.49805566601303 14.25 15.25
transform.rotateInPlace(1.25)
print_(transform, "rotateInPlace")
// CHECK-NEXT: rotateInPlace 14.8571158016574 16.1214227834083 -5.86439340905272 -6.49805566601303 14.25 15.25
