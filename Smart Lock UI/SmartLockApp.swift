import SwiftUI
import FirebaseCore

@main
struct SmartLockApp: App {
    init() {
        FirebaseApp.configure()
    }

    var body: some Scene {
        WindowGroup {
            LoginView()
        }
    }
}
