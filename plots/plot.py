import numpy as np
import matplotlib.pyplot as plt

import topology as tp

"""
Get the list of x or y corrindates for a list of
dictionaries of coordinates (see topology.py for a format reference)

Args:
    l: List of dictionaries of coordinates
    type: 'x' or 'y'

Returns:
    List of 'x' or 'y' coordinates
"""
def get_cd(l, type):
    def get_id(id):
        for i, d in enumerate(l):
            if d['id'] == id:
                return i
    return [l[get_id(i)][type] for i in range(1, len(l)+1)]

"""
Get dictionary with a given 'id' key

Args:
    l: List of coordinates dictionaries
    id: id key to match

Returns:
    The dictionary with id key equal to id
"""
def get_id(id, l):
    for i, d in enumerate(l):
        if d['id'] == id:
            return d

"""
Add an arrow to a line

Args:
    line:       Line2D object
    position:   x-position of the arrow. If None, mean of xdata is taken
    direction:  'left' or 'right'
    size:       size of the arrow in fontsize points
    color:      if None, line color is taken.
"""
def add_arrow(line, position=None, direction='right', size=20, color=None):
    if color is None:
        color = line.get_color()

    xdata = line.get_xdata()
    ydata = line.get_ydata()

    # if position is None:
    #     position = xdata.mean()
    # print(f"position mean: {position}")
    # # find closest index
    # start_ind = np.argmin(np.absolute(xdata - position))
    start_ind = 0
    if direction == 'right':
        end_ind = start_ind + 1
    else:
        end_ind = start_ind - 1

    line.axes.annotate('',
        xytext=(xdata[start_ind], ydata[start_ind]),
        xy=(xdata[end_ind], ydata[end_ind]),
        arrowprops=dict(arrowstyle="->", color=color),
        size=size
    )

"""
Plot the topology

Args:
    top: A list of coordinates dictionaries from topology.py. Required.
    conn: A list of connections from topology.py. Default None.
    figname: Name of the plot to be saved. Default None.
"""
def plot_topology(top, conn=None, figname=None):
    x = get_cd(top, 'x')
    y = get_cd(top, 'y')

    fig = plt.figure()

    # plotting the points
    plt.plot(x, y, color='gray', linestyle='none', linewidth = 3,
             marker='o', markerfacecolor='gray', markersize=12)

    # add number on top of dot
    for i, c in enumerate(zip(x, y)):
        plt.text(c[0], c[1], str(i+1), color="red", fontsize=12)

    # draw connectinon between dots
    if conn is not None:
        for a_ix, b_ix in conn:
            a = get_id(a_ix, top)
            b = get_id(b_ix, top)
            line_handle = plt.plot([a['x'], b['x']], [a['y'], b['y']], color='gray')[0]
            add_arrow(line_handle)

    # Hide ticks from axes
    # plt.xticks([], [])
    # plt.yticks([], [])

    # show plot
    plt.show()
    # save plot
    if figname is not None:
        fig.savefig(figname)

# plot_topology(tp.random_topology_1, tp.random_topology_1_conn, "random_topology_1")
# plot_topology(tp.linear_topology, conn=tp.linear_topology_conn, figname="linear_topology.png")
# plot_topology(tp.random_topology_2, tp.random_topology_2_conn, "random_topology_2")
plot_topology(tp.random_topology_1_close, tp.random_topology_1_close_conn, "random_topology_1_close")
