# can-lite Examples

Reference application categories. These are **not part of the library** and are
not built or installed by default — they exist to show how a consuming project
implements its own category.

Enable them with:

```bash
cmake --preset host -DCAN_LITE_BUILD_EXAMPLES=On
```

They are also built automatically when `CAN_LITE_BUILD_TESTS=On`, so their unit
tests run in CI.

| Example                          | Category ID | Description                                       |
|----------------------------------|-------------|---------------------------------------------------|
| [`foc_motor/`](foc_motor/)       | `0x2`       | Field-oriented motor control commands and telemetry |

## Using an example

Copy the directory into your own project, rename the namespace and identifiers
to suit, and register the category on your server and client. Nothing in
`can-lite/` depends on `examples/`.

Category identifiers `0x2`–`0xF` are reserved for applications; the value used
by an example carries no meaning outside that example.

See [Extending can-lite with categories](../documents/design/extending-categories.md)
for the authoring guide.
