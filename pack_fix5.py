import os
import shutil
import zipfile

src_dir = r"D:/Development/IEGPerTest/external/IEG_Code/code/WLServerTest/x64/Release"
artifact_dir = r"D:/Development/IEGPerTest/artifacts"
pack_name = "WLServerTest_V6.8.1"
pack_dir = os.path.join(artifact_dir, pack_name)

# Project root
proj_root = r"D:/Development/IEGPerTest/external/IEG_Code/code/WLServerTest"

# Clean/create pack directory
if os.path.exists(pack_dir):
    shutil.rmtree(pack_dir)
os.makedirs(pack_dir)

# Copy files
shutil.copy2(os.path.join(src_dir, "WLServerTest.exe"), pack_dir)
shutil.copy2(os.path.join(proj_root, "RawPacketEngine.dll"), pack_dir)
shutil.copy2(os.path.join(proj_root, "clientConfig.json"), pack_dir)
shutil.copy2(os.path.join(proj_root, "WLServerTest.ini"), pack_dir)
# Copy policies directory (client-side policy default JSONs)
policies_src = os.path.join(proj_root, "policies")
policies_dst = os.path.join(pack_dir, "policies")
if os.path.isdir(policies_src):
    shutil.copytree(policies_src, policies_dst)
else:
    print(f"WARNING: policies source dir not found: {policies_src}")

# Create zip
zip_path = os.path.join(artifact_dir, f"{pack_name}.zip")
if os.path.exists(zip_path):
    os.remove(zip_path)

with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as zf:
    for root, dirs, files in os.walk(pack_dir):
        for f in files:
            full_path = os.path.join(root, f)
            arcname = os.path.relpath(full_path, pack_dir)
            zf.write(full_path, arcname)

# Print sizes
for f in ["WLServerTest.exe", "RawPacketEngine.dll", "clientConfig.json", "WLServerTest.ini"]:
    fp = os.path.join(pack_dir, f)
    size = os.path.getsize(fp)
    print(f"{f}: {size} bytes")

pol_dir = os.path.join(pack_dir, "policies")
if os.path.isdir(pol_dir):
    pol_count = len([n for n in os.listdir(pol_dir) if os.path.isfile(os.path.join(pol_dir, n))])
    print(f"policies/: {pol_count} files")

print(f"\nPackaged: {zip_path}")
print(f"Zip size: {os.path.getsize(zip_path)} bytes")
