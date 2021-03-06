Transient Flow in a 2D Confined Aquifer with a Disc Embedded in A Matrix
========================================================================

Introduction
------------

Butler and Liu (1993) :cite:`pod-Butler_Liu_radially_asymmetric_1993` developed a semi-analytical solution for calculating drawdown in an aquifer system, in which a disc of one material is embedded in a matrix of different hydraulic properties. The problem of interest is the drawdown as a function of location and time due to pumping from a fully penetrating well located either in disc or the matrix. The differences on hydraulic properties between the disc and the matrix can be of any magnitude. The problem is solved analytically in the Laplace space and the drawdown is solved numerically by inversion from the Laplace space to the real space.

Model
-----

Flow within the circular disc (:math:`i =1`) and surrounding matrix (:math:`i =2`)  can be described mathematically by the polar-coordinate form of aquifer flow equations: 

.. math:: \frac{\partial ^2 s_i}{\partial r^2} 
   + \frac{1}{r} \frac{\partial s_i}{\partial r} 
   + \frac{1}{r^2} \frac{\partial^2 s_i}{\partial \theta^2} 
   = \frac{S_i}{T_i} \frac{\partial s_i}{\partial t},

where 
:math:`s_i` [L] is the drawdown in material :math:`i`,
:math:`t` [T] is the time,
:math:`T_i` [L\ :sup:`2`\/T] is the transmissivity of material :math:`i`, and
:math:`S_i` [-] is the storage coefficient of material :math:`i`.

The initial conditions are the same for the disc and the matrix:

.. math:: s_i(r, \theta,0) =0.,  0 \le r < \infty.

The boundary condition at infinite is:

.. math::    s_2(-\infty, \theta, t) =  0.

A pumping well discharging at a constant rate :math:`Q` is assumed at location :math:`(r_{pw}, \theta_{pw})`

.. math:: \lim_{R \rightarrow 0} 2 \pi R T_2 \frac{\partial s_2(R,t)}{\partial R} = -Q,\;\; t>0,

where :math:`R` is the distance between the pumping and observation wells. To ensure the flow continuity, the auxiliary conditions at the matrix-disc interfaces (:math:`r = a`) must be met:

.. math::      s_1(a,\theta,t) = s_2(a,\theta,t),\\
.. math::      T_1\frac{\partial s_1(a,\theta,t)}{\partial r} = T_2\frac{\partial s_2(a,\theta,t)}{\partial r}.\\

Problem Specification
---------------------

Schematic
~~~~~~~~~

The problem configuration is illustrated in the following schematic figure:

.. figure:: schematic/butler_pod_schematic.jpg
    :figclass: align-center
    :width: 600 px

    Figure 1. Schematic of the Butler and Liu pod verification problem


Mesh
~~~~

In numerical simulations, a non-uniform mesh was used to better present the disc (Fig. 2), where the central part of the domain has been refined to better represent the disc. The grid spacing increases geometrically toward the domain boundaries.

.. figure:: pod_mesh.jpg
    :figclass: align-center
    :width: 600 px

    Figure 2. Mesh of the Butler and Liu's pod verification problem


Variables
~~~~~~~~~

	Transmissivity: :math:`\;\; T_1 = 0.0011574 \; m2/s`; :math:`T_2 = 0.011574 \;m2/s`;

	Storativity: :math:`\;\; S_1 = S_2 = 2\times 10^{-4}`;

	Pumping rate: :math:`\;\; Q = 1000 \;m3/day (= 0.011574 \;m3/s)`;

	Radius of the disc: :math:`\;\; d = 18 \;m`;

	Pumping well location :math:`\;\; (600 \;m, 0^o)` in the polar coordinate with its origin at the center of the disc;

	Observation well locations :math:`\;\; (40\; m, 60^o)` and :math:`(360 \; m, 120^o)`.

Initial conditions: zero drawdown everywhere in the domain.

Boundary conditions: zero drawdown on four boundaries.


Results and Comparison
----------------------

.. _Plot_ButlerPod2D:

Plot  Analytic Solution and Amanzi Results
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. plot:: amanzi_butler_pod_2d.py
   :align: center


The comparison shows that the results from the Amanzi model match the analytical solution very well at early time, and they deviate when the effect of pumping hits the constant head boundary of the domain. Note that, the analytical solution was developed for unbounded domain, and therefore it is expected that the two solutions will deviate each other at later time.
To show that such a deviation is indeed caused by the boundary effect, we also conducted numerical simulations using
FEHM, a widely used numerical simulator for simulating heat and mass flow in subsurface environment :cite:`pod-Zyvoloski_FEHM_summary_1997`. It is showed that the results from Amanzi are almost the same as those from FEHM, see :cite:`pod-Lu_Harp_Birdsell_benchmarking_2014` for detailed comparison.


References
----------

.. bibliography:: /bib/ascem.bib
   :filter: docname in docnames
   :style:  alpha
   :keyprefix: pod-

About
-----

* Directory: testing/verification/flow/transient/butler_pod_2d


* Authors:  Zhiming Lu (zhiming@lanl.gov),  Dylan Harp (dharp@lanl.gov)

* Maintainer(s):  Zhiming Lu,  Dylan Harp

* Input Files:

  * amanzi_butler_pod_2d.xml

     * Spec: Version 2.0
     * Mesh: mesh_cylinder.exo
     * Runs

* Analytical Solutions

  * Directory: analytic/

  * Executable: butler_pod.x, compiled from FORTRAN code under the Linux environment.

  * Input Files:

     * obs.dat,  specifying parameters for observation wells.
     * anal.dat, specifying other parameters such as the number of time steps, and so on.

  * Output Files:

     * drdn.res,  drawdown as a function of time for all observation wells.


Status
~~~~~~

The analytical solution was solved using a FORTRAN code modified from the original code from Greg Ruskauf.
We may need to implement the algorithm by ourselves or get permission from Greg Ruskauf for using the code.
As the flow problem was solved analytically in the Laplace transformed space, one needs to implement
numerical inversion from the Laplace transformed space back to the real space.

