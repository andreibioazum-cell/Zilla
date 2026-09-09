if env["target"] == "editor":
    gradle_process += ["generateGodotEditor"]  # было 3 задачи
...
result = subprocess.run(...)
if result.returncode != 0:
    sys.exit(result.returncode) # раньше было тихо
