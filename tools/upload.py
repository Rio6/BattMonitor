Import("env")
import os

address = env.Dump("UPLOAD_PORT").replace('\'', '')

def ssh_upload(target, source, env):
    print(f"Attempting to upload firmware via SSH to {address}")
    firmware = str(source[0])
    os.system('scp '+firmware+' root@'+address+':/root/firmware.hex')
    os.system('ssh root@'+address+' run-avrdude /root/firmware.hex')
    exit()

env.AddPreAction("upload", ssh_upload)

