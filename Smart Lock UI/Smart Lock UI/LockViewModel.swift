// FILE: LockViewModel.swift
import Foundation
import Combine            // <-- required for ObservableObject / @Published
import SwiftUI
import FirebaseAuth
import FirebaseDatabase

@MainActor
final class LockViewModel: ObservableObject {

    // MARK: - Published UI state
    @Published var role: String = ""
    @Published var lockA: Bool = false
    @Published var lockB: Bool = false
    @Published var statusMessage: String = "Connecting…"
    @Published var isLoading: Bool = true

    // MARK: - Config
    let deviceId: String = "esp32-01"

    // MARK: - Firebase
    private let dbRef = Database.database().reference()

    // MARK: - Derived permissions
    var isAdmin: Bool { role == "admin" }
    var canOperate: Bool { role == "admin" || role == "operator" }

    // MARK: - Lifecycle
    func start() {
        guard let uid = Auth.auth().currentUser?.uid else {
            statusMessage = "Not logged in"
            return
        }

        // Load role once
        dbRef.child("roles/\(uid)/\(deviceId)")
            .observeSingleEvent(of: .value) { [weak self] snap in
                Task { @MainActor in
                    self?.role = (snap.value as? String) ?? "operator"
                }
            }

        // Live device state
        dbRef.child("devices/\(deviceId)/state")
            .observe(.value) { [weak self] snap in
                guard let self = self else { return }
                if let dict = snap.value as? [String: Any] {
                    let a = dict["lockA"] as? Bool ?? false
                    let b = dict["lockB"] as? Bool ?? false
                    Task { @MainActor in
                        self.lockA = a
                        self.lockB = b
                        self.statusMessage = "Lock A: \(a ? "Locked" : "Unlocked") | Lock B: \(b ? "Locked" : "Unlocked")"
                        self.isLoading = false
                    }
                } else {
                    Task { @MainActor in
                        self.statusMessage = "Waiting for device…"
                    }
                }
            }
    }

    // MARK: - Commands
    func sendPulse(target: String, pulseMs: Int = 500) {
        guard canOperate else {
            statusMessage = "Insufficient privileges"
            return
        }
        guard let uid = Auth.auth().currentUser?.uid else { return }

        let cmdRef = dbRef.child("commands").child(deviceId).child("lastCmd")
        let payload: [String: Any] = [
            "target": target,      // "A" | "B" | "BOTH"
            "pulseMs": pulseMs,
            "issuedBy": uid,
            "done": false
        ]

        cmdRef.setValue(payload) { [weak self] error, _ in
            if let error = error {
                Task { @MainActor in
                    self?.statusMessage = "Error: \(error.localizedDescription)"
                }
                return
            }
            Task { @MainActor in
                self?.statusMessage = "Command sent to \(target)"
            }

            // Optional: wait for confirmation (done == true) then clear observer
            cmdRef.child("done").observe(.value) { [weak self] snap in
                if let done = snap.value as? Bool, done {
                    Task { @MainActor in
                        self?.statusMessage = "ESP32 confirmed command"
                    }
                    cmdRef.child("done").removeAllObservers()
                }
            }
        }
    }
}
