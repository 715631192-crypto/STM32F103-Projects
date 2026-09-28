param(
    [string]$BrokerUser = "door-001",
    [Parameter(Mandatory = $true)]
    [string]$BrokerPassword,
    [string]$CertificateName = "smartlock.local"
)

$ErrorActionPreference = "Stop"
$CloudRoot = Split-Path -Parent $PSScriptRoot
$ConfigRoot = Join-Path $CloudRoot "mosquitto"
$CertRoot = Join-Path $ConfigRoot "certs"
$PasswordFile = Join-Path $ConfigRoot "passwords"

New-Item -ItemType Directory -Force -Path $CertRoot | Out-Null

openssl req -x509 -newkey rsa:3072 -sha256 -days 3650 -nodes `
    -keyout (Join-Path $CertRoot "ca.key") `
    -out (Join-Path $CertRoot "ca.crt") `
    -subj "/CN=Smart Lock Local CA"
openssl req -newkey rsa:3072 -nodes `
    -keyout (Join-Path $CertRoot "server.key") `
    -out (Join-Path $CertRoot "server.csr") `
    -subj "/CN=$CertificateName"
openssl x509 -req -sha256 -days 825 `
    -in (Join-Path $CertRoot "server.csr") `
    -CA (Join-Path $CertRoot "ca.crt") `
    -CAkey (Join-Path $CertRoot "ca.key") `
    -CAcreateserial -out (Join-Path $CertRoot "server.crt")

New-Item -ItemType File -Force -Path $PasswordFile | Out-Null
docker run --rm -v "${ConfigRoot}:/work" eclipse-mosquitto:2 `
    mosquitto_passwd -b /work/passwords $BrokerUser $BrokerPassword

Write-Host "Created TLS material and Mosquitto password database."
Write-Host "Copy .env.example to .env, replace every placeholder, then run docker compose up -d."
