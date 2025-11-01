import SwiftUI
import FirebaseAuth

struct LoginView: View {
    @State private var email = ""
    @State private var password = ""
    @State private var errorMsg = ""
    @State private var isLoggedIn = false

    var body: some View {
        if isLoggedIn {
            MainView()
        } else {
            VStack(spacing: 20) {
                Text("ESP32 Smart Lock")
                    .font(.largeTitle).bold()
                TextField("Email", text: $email)
                    .autocapitalization(.none)
                    .textFieldStyle(RoundedBorderTextFieldStyle())
                    .padding(.horizontal)
                SecureField("Password", text: $password)
                    .textFieldStyle(RoundedBorderTextFieldStyle())
                    .padding(.horizontal)

                Button("Login") {
                    login()
                }
                .buttonStyle(.borderedProminent)
                .padding()

                if !errorMsg.isEmpty {
                    Text(errorMsg)
                        .foregroundColor(.red)
                        .multilineTextAlignment(.center)
                        .padding()
                }
            }
        }
    }

    private func login() {
        Auth.auth().signIn(withEmail: email, password: password) { result, error in
            if let error = error {
                errorMsg = error.localizedDescription
                return
            }
            isLoggedIn = true
        }
    }
}
