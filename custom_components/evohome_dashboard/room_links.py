"""Cloud-name-to-RAMSES-zone matching without assuming ordering.

Cloud zone IDs are vendor identifiers, not the 00..0B RAMSES RF indexes.
Only exact controller 0004 zone-name evidence can link automatically.
Users may explicitly select a known cloud zone without entering names as JSON.
"""
from __future__ import annotations

import re

LINKS_OPTION = "rf_zone_links"
_INDEX = re.compile(r"0[0-9AB]\Z")


def valid_index(value: str) -> bool:
    return isinstance(value, str) and _INDEX.fullmatch(value.upper()) is not None


def valid_links(value: object) -> dict[str, dict[str, str]]:
    if value is None:
        return {}
    if not isinstance(value, dict) or len(value) > 12:
        raise ValueError("RF zone links must be an object of at most 12 indexes")
    result = {}
    for index, ref in value.items():
        if not valid_index(index) or not isinstance(ref, dict):
            raise ValueError("Invalid RF zone link")
        if set(ref) != {"system_id", "zone_id"}:
            raise ValueError("Unexpected cloud zone reference")
        sid, zid = ref["system_id"], ref["zone_id"]
        if not all(isinstance(item, str) and 1 <= len(item) <= 64 for item in (sid, zid)):
            raise ValueError("Invalid cloud zone identifier")
        result[index.upper()] = {"system_id": sid, "zone_id": zid}
    return result


def cloud_zones(snapshot: object) -> list[dict[str, str]]:
    """Read only the secret-free model snapshot already used by the Rooms tab."""
    if not isinstance(snapshot, dict):
        return []
    result = []
    for location in snapshot.get("locations", []):
        if not isinstance(location, dict):
            continue
        for system in location.get("systems", []):
            if not isinstance(system, dict):
                continue
            for zone in system.get("zones", []):
                if not isinstance(zone, dict):
                    continue
                sid, zid, name = system.get("id"), zone.get("id"), zone.get("name")
                if (all(isinstance(value, (str, int)) and not isinstance(value, bool)
                        for value in (sid, zid)) and isinstance(name, str) and name.strip()):
                    sid, zid = str(sid), str(zid)
                    if 1 <= len(sid) <= 64 and 1 <= len(zid) <= 64:
                        result.append({"system_id": sid, "zone_id": zid,
                                       "name": name.strip()})
    return result


def resolve_names(
    cloud_snapshot: object,
    explicit_links: dict[str, dict[str, str]],
    manual_names: dict[int, str],
    radio_names: dict[int, str],
) -> tuple[dict[int, str], dict[int, dict]]:
    """Use verified stable identifiers or unique exact RF 0004 name matches.

    Never infer RF numbering from cloud list position or cloud numeric IDs.
    A duplicated cloud room name makes automatic matching ambiguous and fails
    closed. An old/deleted cloud link is displayed as unlinked, not guessed.
    """
    zones = cloud_zones(cloud_snapshot)
    by_id = {(z["system_id"], z["zone_id"]): z for z in zones}
    by_name = {}
    for zone in zones:
        by_name.setdefault(zone["name"].casefold(), []).append(zone)

    names = {}
    metadata = {}
    for index in set(manual_names) | set(radio_names) | {
        int(key, 16) for key in explicit_links
    }:
        key = f"{index:02X}"
        link = explicit_links.get(key)
        linked = by_id.get((link["system_id"], link["zone_id"])) if link else None
        if linked is not None:
            names[index] = linked["name"]
            metadata[index] = {"source": "selected", **{
                "system_id": linked["system_id"], "zone_id": linked["zone_id"]
            }}
        elif index in manual_names:
            names[index] = manual_names[index]
            metadata[index] = {"source": "manual"}
        elif link is not None:
            metadata[index] = {"source": "missing_cloud_zone"}
        elif index in radio_names:
            matching = by_name.get(radio_names[index].casefold(), [])
            if len(matching) == 1:
                match = matching[0]
                names[index] = match["name"]
                metadata[index] = {"source": "rf_0004", "system_id": match["system_id"],
                                   "zone_id": match["zone_id"]}
            else:
                metadata[index] = {"source": "ambiguous" if matching else "unmatched",
                                   "rf_name": radio_names[index]}
    return names, metadata


def set_link(
    existing: object, index: str, system_id: str, zone_id: str, snapshot: object
) -> dict[str, dict[str, str]]:
    """Produce validated, conflict-free options; no cloud or RF writes."""
    if not valid_index(index):
        raise ValueError("RF zone index must be 00..0B")
    links = valid_links(existing)
    index = index.upper()
    if not system_id and not zone_id:
        links.pop(index, None)
        return links
    ref = (system_id, zone_id)
    if not any((z["system_id"], z["zone_id"]) == ref for z in cloud_zones(snapshot)):
        raise ValueError("Selected room is not a configured Evohome cloud zone")
    if any(idx != index and (item["system_id"], item["zone_id"]) == ref
           for idx, item in links.items()):
        raise ValueError("This cloud room is already linked to another RF zone")
    links[index] = {"system_id": system_id, "zone_id": zone_id}
    return links
