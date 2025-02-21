import subprocess


def get_git_info():
    try:
        shortSHA = ( subprocess.check_output(["git", "rev-parse", "--short" , "HEAD"]).strip().decode("utf-8") )    
        branch = ( subprocess.check_output(["git", "rev-parse", "--abbrev-ref", "HEAD"]).strip().decode("utf-8") )

        try:
            tag = subprocess.check_output(["git", "describe", "--tags", "--exact-match"]).strip().decode("utf-8")
        except subprocess.CalledProcessError:
            tag = "untagged"
            
        dirtyFlag = subprocess.call(["git", "diff-index", "--quiet", "HEAD", "--"]) != 0
        
        return branch, shortSHA, tag, dirtyFlag
    except Exception as e:
        print(f"Error getting GIT info: {e}")
        return "unknown", "unknown", "unknown", False
    
def generate_build_flags(env):
    branch, shortSHA, tag, dirtyFlag = get_git_info()
    dirty = "dirty" if dirtyFlag else "clean"
    version_string = f"{tag} {shortSHA}-{dirty}"
    env.Append(CPPDEFINES=[("VERSION_STRING", env.StringifyMacro(version_string))])
    env.Append(CPPDEFINES=[("VERSION_BRANCH", env.StringifyMacro(branch))])
    print(f"Version: {version_string}")
    print(f"Branch: {branch}")
    
Import("env")
generate_build_flags(env)