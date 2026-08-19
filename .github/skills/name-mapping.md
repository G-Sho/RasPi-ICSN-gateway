# Name mapping

## Role

`NameMapper` manages the conversion between ICSN content names and CEFORE URI names used by the local runtime.

## Behaviors

- `addScheme()` adds `ccnx:/` when the name is missing a scheme
- `removeScheme()` removes the scheme when CEFORE traffic is normalized back into the ICSN name space
- `stripCeforeComponents()` removes CEFORE-specific TLV-like suffix components appended by the CEFORE stack before the gateway forwards the name to the ICSN side

## Guidance

Document the name mapping as a translation layer at the gateway boundary, not as an NDN metadata feature owned by the gateway itself.
