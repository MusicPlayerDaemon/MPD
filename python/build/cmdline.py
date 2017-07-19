def concatenate_cmdline_variables(src, names):
    """Find duplicate variable declarations on the given source list, and
    concatenate the values of those in the 'names' list."""

    # the result list being constructed
    dest = []

    # a map of variable name to destination list index
    positions = {}

    for item in src:
        i = item.find('=')
        if i > 0:
            # it's a variable
            name = item[:i]
            if name in names:
                # it's a known variable
                if name in positions:
                    # already specified: concatenate instead of
                    # appending it
                    dest[positions[name]] += ' ' + item[i + 1:]
                    continue
                else:
                    # not yet seen: append it and remember the list
                    # index
                    positions[name] = len(dest)
        dest.append(item)

    return dest
