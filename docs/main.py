from urllib.parse import quote


def define_env(env):
    def extra_value(name):
        variables = getattr(env, "variables", {})
        if isinstance(variables, dict):
            if name in variables:
                return variables[name]

            extra = variables.get("extra")
            if isinstance(extra, dict) and name in extra:
                return extra[name]

            config = variables.get("config")
            if config is not None:
                extra = getattr(config, "extra", None)
                if isinstance(extra, dict) and name in extra:
                    return extra[name]
                if hasattr(config, "get"):
                    extra = config.get("extra", {})
                    if isinstance(extra, dict) and name in extra:
                        return extra[name]

        raise KeyError(f"missing mkdocs extra.{name}")

    source_repo = str(extra_value("source_repo")).rstrip("/")
    source_ref = str(extra_value("source_ref"))

    @env.macro
    def source_file(path, line=None, end_line=None):
        source_path = str(path).strip()
        if not source_path:
            raise ValueError("source_file() requires a non-empty path")

        source_type = "tree" if source_path.endswith("/") else "blob"
        source_path = source_path.strip("/")
        encoded_path = quote(source_path, safe="/._-+~*")
        url = f"{source_repo}/{source_type}/{source_ref}/{encoded_path}"

        if line is None:
            if end_line is not None:
                raise ValueError("source_file() end_line requires line")
            return url

        start = int(line)
        if start < 1:
            raise ValueError("source_file() line must be positive")

        if end_line is None:
            return f"{url}#L{start}"

        end = int(end_line)
        if end < start:
            raise ValueError("source_file() end_line must be >= line")
        return f"{url}#L{start}-L{end}"
