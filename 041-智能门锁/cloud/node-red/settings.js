const uiUser = process.env.NODE_RED_UI_USER || "admin";
const uiPasswordHash = process.env.NODE_RED_UI_PASSWORD_HASH || "";

if (!/^\$2[aby]\$/.test(uiPasswordHash)) {
    throw new Error("NODE_RED_UI_PASSWORD_HASH must be a bcrypt hash");
}

module.exports = {
    flowFile: "flows.json",
    credentialSecret: process.env.NODE_RED_CREDENTIAL_SECRET,
    uiPort: process.env.PORT || 1880,
    httpAdminRoot: "/admin",
    httpNodeRoot: "/",
    adminAuth: {
        type: "credentials",
        users: [{ username: uiUser, password: uiPasswordHash, permissions: "*" }]
    },
    httpNodeAuth: { user: uiUser, pass: uiPasswordHash },
    functionGlobalContext: {
        crypto: require("crypto")
    },
    contextStorage: {
        default: {
            module: "localfilesystem"
        }
    },
    editorTheme: {
        projects: { enabled: false }
    },
    logging: {
        console: { level: "info", metrics: false, audit: false }
    }
};
