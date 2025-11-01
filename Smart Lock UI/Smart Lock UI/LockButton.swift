import SwiftUI


struct LockButton: View {
let title: String
let isLocked: Bool
let action: () -> Void


var body: some View {
Button(action: action) {
HStack(spacing: 10) {
Image(systemName: isLocked ? "lock.fill" : "lock.open.fill")
Text(labelText)
.font(.headline)
.bold()
}
.frame(maxWidth: .infinity)
.padding()
.background(isLocked ? Color.red.opacity(0.15) : Color.green.opacity(0.15))
.cornerRadius(12)
}
.buttonStyle(.plain)
}


private var labelText: String {
// Title + dynamic status from Firebase state
"\(title): \(isLocked ? "Locked" : "Unlocked")"
}
}
