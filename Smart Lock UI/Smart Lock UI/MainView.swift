import SwiftUI
import FirebaseAuth


struct MainView: View {
@StateObject private var vm = LockViewModel()


var body: some View {
VStack(spacing: 22) {
Text("Smart Lock Control").font(.title).bold()


// Role indicator
HStack(spacing: 8) {
Circle()
.fill(vm.isAdmin ? Color.green : Color.blue)
.frame(width: 10, height: 10)
Text("Account: \(vm.role.isEmpty ? "unknown" : vm.role)")
.font(.subheadline).foregroundColor(.gray)
}


Spacer(minLength: 12)


// Buttons
LockButton(title: "Lock A", isLocked: vm.lockA) { vm.sendPulse(target: "A") }
LockButton(title: "Lock B", isLocked: vm.lockB) { vm.sendPulse(target: "B") }
LockButton(title: "Both Locks", isLocked: (vm.lockA && vm.lockB)) { vm.sendPulse(target: "BOTH") }


Spacer()


Text(vm.statusMessage)
.font(.footnote)
.foregroundColor(.gray)


Button("Logout") {
try? Auth.auth().signOut()
exit(0)
}
.buttonStyle(.bordered)
}
.padding()
.onAppear { vm.start() }
}
}
