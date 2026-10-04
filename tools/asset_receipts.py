"""Keep build provenance outside RomFS without changing temporary tool outputs."""
from pathlib import Path


def receipt_path(output, project, name='source.json'):
    output, project = Path(output).resolve(), Path(project).resolve()
    try:
        relative = output.relative_to(project / 'romfs')
    except ValueError:
        return output / name
    return project / 'content/asset-receipts' / relative / name


def receipt_entries(output, project, name='source.json'):
    """Receipt filenames allowed beside assets in an isolated temporary output."""
    return {name} if receipt_path(output, project, name).parent == Path(output).resolve() else set()
