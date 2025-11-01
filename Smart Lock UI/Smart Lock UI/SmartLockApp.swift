//
//  SmartLockApp.swift
//  Smart Lock UI
//
//  Created by Krisof  Koczor  on 2025. 10. 29..
//


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
